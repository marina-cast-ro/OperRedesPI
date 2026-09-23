#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

// Lee config.txt y precarga los vecinos en la MMU con saveRoute()
int parseConfigAndPreload(const char *filename);

#endif