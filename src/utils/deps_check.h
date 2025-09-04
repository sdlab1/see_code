// src/utils/deps_check.h
#ifndef SEE_CODE_DEPS_CHECK_H
#define SEE_CODE_DEPS_CHECK_H

// Dependency checking functions
int deps_check_all(void);
int deps_check_termux_environment(void);
int deps_check_termux_gui_app(void);
int deps_check_libraries(void);
int deps_check_fonts(void);
int deps_check_permissions(void);

// Individual dependency checks
int deps_check_gles2(void);
int deps_check_freetype(void);
// int deps_check_cjson(void); // REMOVED
int deps_check_pthread(void);
int deps_check_termux_gui_c(void); // NEW (если используется)

// Utility functions
void deps_print_report(void);
const char* deps_get_missing_solution(const char* component);

#endif // SEE_CODE_DEPS_CHECK_H
