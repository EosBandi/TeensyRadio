/**
 * @file print.h
 * 
 * Prototypes for print commands
 * 
 */

/**
 * @brief Print to serial 0 (TODO: Rename it to something more meaningful)
 * 
 * @param format normal printf formating
 * @param ...  additional parameters for printing
 */
void s1printf(const char *format, ...);


/**
 * @brief Print out debug information, only when DEBUG is defined, otherwise does nothing
 * 
 * @param format normal printf formatting
 * @param ... variables to print
 */
void debug(const char *format, ...);