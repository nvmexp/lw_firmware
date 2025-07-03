// This partition is fully authorized to setup and control
// all ACLs.  These is generally set as the root partition
LwU8 parentPartition = -1;

LibosStatus LwosRootDesignateParent(LwU64 partition)
{
    // This is a one-time call to bind the parent partition
    if (parentPartition != -1)
        return LibosErrorAccess;

    if (partition >= 64)
        return LibosErrorArgument;

    parentPartition = partition;

    return LibosOk;
}

LibosStatus LwosRootPartitionGrantAcl(LwU32 partitionId, LwU64 spawnGrantMask, LwU64 reclaimGrant)
{
    // ACLs may only be set/updated from the parent partition
    if (fromPartition != parentPartition)
        return LibosErrorAccess;

    if (partitionId >= 64)
        return LibosErrorArgument;

    partitions[partitionId].grantSpawn = spawnGrantMask;
    partitions[partitionId].grantReclaim = reclaimGrant;
}
