# win_assign_drive_letter.ps1
#
# Automatically assign drive letters to disk partitions that do not have one.
#
# Usage (run as Administrator):
#   powershell -ExecutionPolicy Bypass -File win_assign_drive_letter.ps1
#
# The script skips System, Recovery, and ESP (EFI System Partition) partitions
# to avoid disturbing critical boot components.

#Requires -RunAsAdministrator

param(
    # When specified, only print what would be done without making changes.
    [switch]$WhatIf
)

function Get-NextAvailableLetter {
    $used = (Get-PSDrive -PSProvider FileSystem).Name |
            Where-Object { $_.Length -eq 1 }
    foreach ($c in [char[]]([char]'C'..[char]'Z')) {
        if ($c -notin $used) { return $c }
    }
    return $null
}

# Partition types to skip (GUID strings from the GPT spec / Windows)
$skipTypes = @('System', 'Recovery', 'Unknown')

$partitions = Get-Partition |
    Where-Object {
        # No drive letter assigned
        (-not $_.DriveLetter) -and
        # Not a type we should leave alone
        ($_.Type -notin $skipTypes) -and
        # Skip ESP (EFI System Partition) by GUID
        ($_.GptType -ne '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}') -and
        # Must be a recognised, usable partition
        (-not $_.IsOffline)
    }

if ($partitions.Count -eq 0) {
    Write-Host "[INFO] All partitions already have drive letters (or no assignable partitions found)."
    exit 0
}

foreach ($part in $partitions) {
    $letter = Get-NextAvailableLetter
    if ($null -eq $letter) {
        Write-Warning "[WARN] No available drive letters remain. Stopping."
        break
    }

    $desc = "Disk $($part.DiskNumber), Partition $($part.PartitionNumber) (Size: $([math]::Round($part.Size/1GB,1)) GB)"
    if ($WhatIf) {
        Write-Host "[WHATIF] Would assign $letter`: to $desc"
    } else {
        try {
            Set-Partition -DiskNumber $part.DiskNumber `
                          -PartitionNumber $part.PartitionNumber `
                          -NewDriveLetter $letter
            Write-Host "[OK] Assigned $letter`: to $desc"
        } catch {
            Write-Warning "[FAIL] Could not assign $letter`: to ${desc}: $_"
        }
    }
}
