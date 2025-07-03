#!/bin/bash

##################################################################################################
# Cronjob command:
# #Adding a cronjob for updating dvs_package.h as part of FmodUpdate
# 0 */6 * * * /home/scratch.sw-rmtest-autoCheckin_sw/fmodUpdate_hsmb/cron_cronFmodupdChipsa_hsmb.sh
#
#
##################################################################################################



declare -A GpuDataMap=(
[gh100]="TOT_FMODEL_REPO=/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gh100,LITTER=ghlit1"
[gh202]="TOT_FMODEL_REPO=/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu,LITTER=ghlit3"
[ls10]="TOT_FMODEL_REPO=/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_ls10,LITTER=lslit2"
[ad102]="TOT_FMODEL_REPO=/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_ad10x,LITTER=adlit1"
[gb100]="TOT_FMODEL_REPO=/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu,LITTER=gblit1"
)

dvs_package_path="${P4ROOT}/sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/*/"
perl_script_path="//sw/dev/gpu_drv/chips_a/diag/mods/gpu/tests/rm/tools/autoFmodUpdate/bin/cronFmodupdChipsa_hsmb.pl"
golden_path="/home/scratch.sw-rmtest-autoCheckin_sw/fmodUpdate_hsmb/"
logfile="/home/scratch.sw-rmtest-autoCheckin_sw/fmodUpdate_hsmb/logs/cronFmodupdChipsa_hsmb_sh.log"

function parseFmodelCL()
{
    #this is for golden file checking
    if [[ -f /home/lw/utils/hsmb/bin/last_promotion_info.pl ]]
    then
        /home/lw/utils/hsmb/bin/last_promotion_info.pl > ${golden_path}/golden_cl.txt
    else
        echo "file doesn't exists!" >> $logfile
        return 1
    fi

    golden_output_path="${golden_path}/golden_cl.txt"

    declare -A MYMAP 

    start=`grep -n -w  "golden_2001_cls" ${golden_output_path} | cut -d ":" -f 1`
    end=`grep -n -w  "last chips_sw to chips_hw promotion info" ${golden_output_path} | cut -d ":" -f 1`


    start="$(( $start + 1 ))"
    end="$(( $end - 2 ))"


    for (( i=${start}; i<=${end}; i++))
    do
        temp1=`sed "${i}q;d" ${golden_output_path} |  cut -d " " -f 1 `
        temp2=`sed "${i}q;d" ${golden_output_path} |  cut --complement -d " " -f 1 | tr -dc '0-9'`
        MYMAP[${temp1}]+=${temp2}
    done

    for FmodelG in "${!GpuDataMap[@]}" 
    do
        temp3=`echo ${GpuDataMap[$FmodelG]} | cut -d "," -f 1 | cut --complement -d "=" -f 1 | rev | cut -d "/" -f1  | rev | sed 's/"//g'`
        if test "${MYMAP[$temp3]+isset}"
        then
            golden_cl_number=${MYMAP[$temp3]}
            echo -e "\n\nChip: $FmodelG      Golden CL: $golden_cl_number" >> $logfile


            # Commenting this code as DVS has mechanism in place to download Fmodel / RTL packages seperately, Bug 3314558 
            # req_param=${GpuDataMap[$FmodelG]}
            # TOT_FMODEL_REPO=`echo $req_param  | cut -d "," -f 1 | cut --complement -d "=" -f 1 `
            # LITTER=`echo $req_param  | cut --complement -d "," -f 1 | cut --complement -d "=" -f 1 `
            # fmodel_package=${LITTER}"_fmodel/build.tgz"

            #Check if a seperate Fmodel Package exists for this golden CL, if yes then proceed, else skip updating dvs_package.h with this golden CL
            # if [ ! -f "$TOT_FMODEL_REPO/$golden_cl_number/$fmodel_package" ]
            # then
            #     echo "A Seperate Fmodel package for Chip: $FmodelG and Golden CL: $golden_cl_number not found!!" >> $logfile
            #     echo "This CL won't be updated in dvs_package.h for chip: $FmodelG, continuing to next chip" >> $logfile
            #     continue
            # else
            #     echo "A Seperate Fmodel package for Chip: $FmodelG and Golden CL: $golden_cl_number found. Proceeding ahead!!" >> $logfile
            # fi

            #sync latest dvs_package.h
            p4 sync $dvs_package_path/$FmodelG/net/dvs_package.h
            if [[ $? != 0 ]]
            then
                echo "Error while syncing file $dvs_package_path/$FmodelG/net/dvs_package.h. Exiting" >> $logfile
                exit 1
            fi

            #Check if the dvs_package.h already has this CL updated, if yes, then no need to update
            if grep -q "\b$golden_cl_number\b" $dvs_package_path/$FmodelG/net/dvs_package.h
            then
                echo "Chip: $FmodelG Golden CL: $golden_cl_number already present in $dvs_package_path/$FmodelG/net/dvs_package.h" >> $logfile
            else
                echo "Chip: $FmodelG Golden CL: $golden_cl_number not present in $dvs_package_path/$FmodelG/net/dvs_package.h" >> $logfile
                echo "Calling $perl_script_path to update with the latest golden_cl" >> $logfile
                ${P4ROOT}/$perl_script_path $golden_cl_number $FmodelG
            fi
        fi
    done
}


echo "Starting the cronjob at: "`date` > $logfile
parseFmodelCL
