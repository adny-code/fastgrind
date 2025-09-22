# Fastgrind Questions List

## Troubleshooting

- **Linking errors**: Ensure all wrap flags are properly specified
- **Missing symbols**: Check that `-Wl,--export-dynamic` is used in automatic instrumentation
- **System header conflicts**: Verify exclusion lists in automatic instrumentation
- **Compilation failures**: Check that `FASTGRIND_INSTRUMENT` is defined for automatic mode
- **TCMalloc/JEMalloc conflicts**: Add `-DFASTGRIND_TC_MALLOC` or `-DFASTGRIND_JE_MALLOC` flags


## Compile Questions


## Linke Questions

- **malloc/free function undefined**: Ensure all wrap flags are properly specified
- **malloc/free function multi defined**: Ensure all wrap flags are properly specified

## Using Questions