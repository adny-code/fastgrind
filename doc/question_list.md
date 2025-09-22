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