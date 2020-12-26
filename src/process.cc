/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "process.h"
#include "process_commandline.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <limits>
#include <errno.h>
#include <set>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>

void GetRawProcessList( DWORD* rootPid, ProcessInfo process_info[1024], uint32_t* process_count, DWORD* process_data_flags) {
  *process_count = 0;

  // std::set<DWORD> invalid;

  std::unordered_multimap <DWORD,DWORD > parentToChildrenMap;
  std::unordered_map <DWORD, DWORD > childrenToParentMap;
  std::unordered_map <DWORD, std::reference_wrapper< ProcessInfo > > pidToProcessInfo;

  /*
    Create set for invalid processes
    Check if process is valid
    Put invalid process into the list
    Invalidate process if its ppid exists in the list

    Invalid:
      - If ppid was changed
      - If ppid exists in the list of invalid processes
  */

  // Fetch the PID and PPIDs
  PROCESSENTRY32 process_entry = { 0 };
  DWORD parent_pid = 0;
  HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  process_entry.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(snapshot_handle, &process_entry))
  {
    do
    {
      if (process_entry.th32ProcessID != 0)
      {
        auto pinfo = new ProcessInfo();

        pinfo->ppidChanged = processPpidChanged( process_entry.th32ProcessID, process_entry.th32ParentProcessID );
        pinfo->pid = process_entry.th32ProcessID;
        pinfo->ppid = process_entry.th32ParentProcessID;

        if (MEMORY & *process_data_flags) {
          GetProcessMemoryUsage( pinfo );
        }

        if (COMMANDLINE & *process_data_flags) {
          GetProcessCommandLine( pinfo );
        }

        strcpy( pinfo->name, process_entry.szExeFile);

        pidToProcessInfo.insert( std::make_pair( process_entry.th32ProcessID, std::ref( *pinfo ) ) );
        childrenToParentMap.insert( std::make_pair( process_entry.th32ProcessID, process_entry.th32ParentProcessID ) );
        parentToChildrenMap.insert( std::make_pair( process_entry.th32ParentProcessID, process_entry.th32ProcessID ) );

        // auto a = pidToProcessInfo.at( process_entry.th32ProcessID );
        // std::cout << a.get().pid << std::endl;

        (*process_count)++;
      }
    } while (*process_count < 1024 && Process32Next(snapshot_handle, &process_entry));

  }

  CloseHandle(snapshot_handle);

  std::unordered_map<DWORD, std::reference_wrapper< ProcessInfo >>::iterator it;
  uint32_t c = 0;
  for (it = pidToProcessInfo.begin(); it != pidToProcessInfo.end(); it++, c++)
  {
    process_info[ c ] = it->second.get();
  }

}

void GetProcessMemoryUsage( ProcessInfo *process_info ) {
  DWORD pid = process_info->pid;
  HANDLE hProcess;
  PROCESS_MEMORY_COUNTERS pmc;

  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);

  if (hProcess == NULL) {
    return;
  }

  if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
    process_info->memory = (DWORD)pmc.WorkingSetSize;
  }
}

// Per documentation, it is not recommended to add or subtract values from the FILETIME
// structure, or to cast it to ULARGE_INTEGER as this can cause alignment faults on 64-bit Windows.
// Copy the high and low part to a ULARGE_INTEGER and peform arithmetic on that instead.
// See https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
ULONGLONG GetTotalTime(const FILETIME* kernelTime, const FILETIME* userTime) {
  ULARGE_INTEGER kt, ut;
  kt.LowPart = (*kernelTime).dwLowDateTime;
  kt.HighPart = (*kernelTime).dwHighDateTime;

  ut.LowPart = (*userTime).dwLowDateTime;
  ut.HighPart = (*userTime).dwHighDateTime;

  return kt.QuadPart + ut.QuadPart;
}

void GetCpuUsage(Cpu* cpu_info, uint32_t* process_index, BOOL first_pass) {
  DWORD pid = cpu_info[*process_index].pid;
  HANDLE hProcess;

  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);

  if (hProcess == NULL) {
    return;
  }

  FILETIME creationTime, exitTime, kernelTime, userTime;
  FILETIME sysIdleTime, sysKernelTime, sysUserTime;
  if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)
    && GetSystemTimes(&sysIdleTime, &sysKernelTime, &sysUserTime)) {
    if (first_pass) {
      cpu_info[*process_index].initialProcRunTime = GetTotalTime(&kernelTime, &userTime);
      cpu_info[*process_index].initialSystemTime = GetTotalTime(&sysKernelTime, &sysUserTime);
    } else {
      ULONGLONG endProcTime = GetTotalTime(&kernelTime, &userTime);
      ULONGLONG endSysTime = GetTotalTime(&sysKernelTime, &sysUserTime);

      cpu_info[*process_index].cpu = 100.0 * (endProcTime - cpu_info[*process_index].initialProcRunTime) / (endSysTime - cpu_info[*process_index].initialSystemTime);
    }
  } else {
    cpu_info[*process_index].cpu = std::numeric_limits<double>::quiet_NaN();
  }

  CloseHandle(hProcess);
}

ULONGLONG processCreationTimeGet(DWORD pid, bool &err ) {
  HANDLE hProcess;

  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);

  if (hProcess == NULL) {
    return 0;
  }

  FILETIME creationTime, exitTime, kernelTime, userTime;
  ULONGLONG ctime = 0;
  if( GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime) ) {
   ctime = ULARGE_INTEGER{ creationTime.dwLowDateTime, creationTime.dwHighDateTime }.QuadPart;
  }
  else {
    err = true;
    errno = GetLastError();
  }

  CloseHandle(hProcess);

  return ctime;
}

bool processPpidChanged( DWORD pid, DWORD ppid )
{
  uint32_t process_count = 0;
  PROCESSENTRY32 process_entry = { 0 };
  HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);

  if( snapshot_handle == INVALID_HANDLE_VALUE)
  return false;

  process_entry.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(snapshot_handle, &process_entry))
  {
    do
    {
      if (process_entry.th32ProcessID != 0 )
      {
        if (process_entry.th32ProcessID == pid )
        {
          return process_entry.th32ParentProcessID == ppid;
        }

        process_count++;
      }
    } while (process_count < 1024 && Process32Next(snapshot_handle, &process_entry));
  }

  CloseHandle(snapshot_handle);

  return false;
}