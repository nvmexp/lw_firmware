define libos_show_tasks
	set $task_count = 0
	set $task_count = (long)&notaddr_task_by_id_count

	if ($task_count == 0)
		echo LIBOS symbols not loaded
	else
		set $idx = 0
		while ($idx < $task_count)
            set $the_task = ((libos_task_t **)task_by_id)[$idx]
            _libos_indent_task
			set $idx = $idx + 1
		end
    end
end

define _libos_indent_task
    printf "[%d] ", $the_task->task_id
    info symbol $the_task
    set $lwrrent_task = (libos_task_t*)$xscratch

    printf "    State: "
    # LIBOS_TASK_STATE_PORT_WAIT
    if ($the_task->task_state & 1)
        printf "LIBOS_TASK_STATE_PORT_WAIT "
    end

    # LIBOS_TASK_STATE_TIMER_WAIT
    if ($the_task->task_state & 2)
        printf "LIBOS_TASK_STATE_TIMER_WAIT "
    end

    # LIBOS_TASK_STATE_TRAPPED
    if ($the_task->task_state & 4)
        printf "LIBOS_TASK_STATE_TRAPPED "
    end

    if ($the_task->task_state == 0)
        printf "LIBOS_TASK_STATE_NORMAL "
    end

    # @todo Check to see if we're in machine mode first
    set $task_pc = $the_task->state.xepc
    if ($lwrrent_task == $the_task)
        set $task_pc = $pc
        printf "(RUNNING) "
    end

    printf "\n"

    if ($the_task->task_state & 2)
        printf "    Timer wakeup sheduled @ %d \n", $the_task->timestamp
    end    

    if ($the_task->task_state & 1)
        set $sleeping_on = $the_task->sleeping_on_shuttle
        if ($sleeping_on == 0)
            printf "    Sleeping on ANY SHUTTLE \n"
        end
        if ($sleeping_on != 0)
            printf "    Sleeping on "
            info symbol $sleeping_on
        end
    end    

    printf "    Isolation Domain: %d\n", $the_task->pasid
    printf "    PC: %x ", $task_pc
    info symbol $task_pc
    printf "        "
    info line *$task_pc
    printf "    last-xcause: %x \t", $the_task->state.xcause
    printf "    last-xtval: %x \n", $the_task->state.xtval
    printf "    Resources:\n"
    
    
	set $resource_count = $the_task->resource_count
    set $resource_id = 0

    while ($resource_id < $resource_count)
        # Actual resource ID's are 1 indexed
        set $resource = $the_task->resources[$resource_id]
        set $resource_id = $resource_id + 1                     

        # Skip the internal trap ports and shuttles
        if (($resource_id < 3) || ($resource_id > 5))
            _libos_indent_resource
        end
    end

    printf "\n"
end



define _libos_indent_resource
    # LIBOS_RESOURCE_PORT_OWNER_GRANT | LIBOS_RESOURCE_PORT_SEND_GRANT
    if (($resource & 3) == 1)
        set $ptr = (libos_port_t *)($resource &~ 7)
        printf "        "
        info symbol $ptr

        # @todo Walk the waiting senders
        set $sender = $ptr->waiting_senders.next
        if ($sender != &$ptr->waiting_senders)          
            while ($sender != &$ptr->waiting_senders)          
                set $sending_shuttle = (libos_shuttle_t*)((long)$sender - (long)&(((libos_shuttle_t*)0)->port_sender_link))
                printf "            waiting send shuttle: "
                info symbol $sending_shuttle
                set $sender = $sender->next
            end
        end
        if ($ptr->waiting_future)
            printf "            waiting recv shuttle: "
            set $waiter = (libos_shuttle_t*)$ptr->waiting_future           
            info symbol $waiter
        end
    end

    # LIBOS_RESOURCE_SHUTTLE_GRANT
    if ($resource & 4)
        set $ptr = (libos_shuttle_t *)($resource &~ 7)
        printf "        "
        info symbol $ptr
        set $payload_address = $ptr->payload_address
        set $payload_size = $ptr->payload_size
        
        # LIBOS_SHUTTLE_STATE_COMPLETE_IDLE
        if ($ptr->state == 1)
            set $status = $ptr->error_code
            printf "            completed (reported)\n"
            set $grantee_shuttle = (libos_shuttle_t*)$ptr->reply_to_shuttle
            if ($grantee_shuttle != 0)
                printf "                REPLY allowed to shuttle    ";
                info symbol $grantee_shuttle
                printf "                REPLY routed to port    ";
                set $grantee_port = $grantee_shuttle->waiting_on_port
                info symbol $grantee_port
            end
        end

        # LIBOS_SHUTTLE_STATE_COMPLETE_PENDING
        if ($ptr->state == 2)
            printf "            completed (pending)\n"
        end        

        # LIBOS_SHUTTLE_STATE_SEND
        if ($ptr->state == 3)
            printf "            async send (%016x, Size = %08x)\n", $payload_address, $payload_size
        end   

        # LIBOS_SHUTTLE_STATE_RECV
        if ($ptr->state == 4)
            printf "            async recv (%016x, %08x)\n", $payload_address, $payload_size
        end   
    end
end
