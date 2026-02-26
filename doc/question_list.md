# Fastgrind Questions List

## Feature Questions
### Why not just override malloc/free

Tcmalloc and jemalloc override malloc/free through global symbols, so they can directly replace the default malloc/free at link time, without the need for -Wl,--wrap options. However, this will cause malloc/free in the static library to be unable to be replaced.

To monitor static library, we choose to use -Wl,--wrap options.


### Why not turn on sys calls (mmap\brk...)





## Compile Questions
- **System header conflicts**: Verify exclusion lists in automatic instrumentation
- **Compilation failures**: Check that `FASTGRIND_INSTRUMENT` is defined for automatic mode
- **TCMalloc/JEMalloc conflicts**: Add `-DFASTGRIND_TC_MALLOC` or `-DFASTGRIND_JE_MALLOC` flags



## Linke Questions
- **malloc/free function undefined**: Ensure all wrap flags are properly specified
- **malloc/free function multi defined**: Ensure all wrap flags are properly specified



## Runtime Questions
- **Coredump**: Check -finstrument-functions-exclude-file-list={}, add all system header folder in exclude list 
- **Missing symbols**: Check that `-Wl,--export-dynamic` is used in automatic instrumentation



## Windows Questions

### New disk recognized by Device Manager but no drive letter is assigned

**Symptom**: Windows Device Manager can detect a newly connected or added disk, but the disk does not appear in File Explorer because no drive letter has been assigned to it.

**Root cause**: Windows only assigns a drive letter automatically when a partition is formatted and mounted. A raw, uninitialized, or previously used disk may be online but have no mount point yet.

**Solutions**:

#### Option 1 – Disk Management GUI (one-time, no scripting)
1. Press `Win + R`, type `diskmgmt.msc`, press **Enter**.
2. Locate the disk that shows as *Online* but has no drive letter.
3. Right-click the volume → **Change Drive Letter and Paths…** → **Add** → choose a letter → **OK**.

#### Option 2 – `diskpart` (command line)
```cmd
diskpart
list disk
select disk <N>        :: replace <N> with the disk number shown
list partition
select partition <M>   :: replace <M> with the partition number
assign letter=E        :: replace E with the desired letter
exit
```

#### Option 3 – PowerShell script (batch / automated)

The `tools/win_assign_drive_letter.ps1` script scans all partitions that lack a drive letter and automatically assigns the next available letter (C–Z), skipping System, Recovery, and EFI partitions.

```powershell
# Preview what would be changed (no writes)
powershell -ExecutionPolicy Bypass -File tools\win_assign_drive_letter.ps1 -WhatIf

# Apply the changes (requires Administrator)
powershell -ExecutionPolicy Bypass -File tools\win_assign_drive_letter.ps1
```

> **Note**: Run PowerShell as Administrator, otherwise `Set-Partition` will fail with an access-denied error.