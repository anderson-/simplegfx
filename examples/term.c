#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simplegfx.h>
#include <simpleterm.h>

static int terminal_initialized = 0;

void execute_command(const char *line) {
    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') line++;

    // Empty line
    if (*line == '\0') {
        return;
    }

    // Parse command and arguments
    char cmd[32];
    const char *args = NULL;

    // Extract command
    int i = 0;
    while (i < 31 && line[i] && line[i] != ' ' && line[i] != '\t') {
        cmd[i] = line[i];
        i++;
    }
    cmd[i] = '\0';

    // Skip to arguments
    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i]) {
        args = &line[i];
    }

    // Execute commands
    if (strcmp(cmd, "echo") == 0) {
        if (args && strcmp(args, "-e") == 0) {
            // Skip -e and find actual text
            while (*args && *args != ' ' && *args != '\t') args++;
            while (*args == ' ' || *args == '\t') args++;

            if (args) {
                // Process escape sequences
                for (const char *p = args; *p; p++) {
                    if (*p == '\\' && *(p+1)) {
                        switch (*(p+1)) {
                            case 'n': gfxt_printf("\n"); break;
                            case 't': gfxt_printf("\t"); break;
                            case 'r': gfxt_printf("\r"); break;
                            case '\\': gfxt_printf("\\"); break;
                            case 'e': gfxt_printf("\x1b"); break; // ESC
                            case '0': case '1': case '2': case '3':
                            case '4': case '5': case '6': case '7':
                                // Simple octal parsing (up to 2 digits)
                                if (*(p+2) >= '0' && *(p+2) <= '7') {
                                    char code = ((*(p+1) - '0') << 3) | (*(p+2) - '0');
                                    gfxt_printf("%c", code);
                                    p++;
                                } else {
                                    gfxt_printf("%c", *(p+1) - '0');
                                }
                                break;
                            default:
                                gfxt_printf("%c", *(p+1)); break;
                        }
                        p++;
                    } else {
                        gfxt_printf("%c", *p);
                    }
                }
            }
            gfxt_printf("\n");
        } else {
            // Regular echo
            if (args) {
                gfxt_printf("%s\n", args);
            } else {
                gfxt_printf("\n");
            }
        }
    }
    else if (strcmp(cmd, "help") == 0) {
        gfxt_printf("\x1b[1;36mSimple Terminal Commands:\x1b[0m\n");
        gfxt_printf("  \x1b[32mecho\x1b[0m \x1b[33m[text]\x1b[0m           - Display text\n");
        gfxt_printf("  \x1b[32mecho\x1b[0m \x1b[33m-e\x1b[0m \x1b[33m[text]\x1b[0m        - Display text with escape sequences\n");
        gfxt_printf("  \x1b[32mhelp\x1b[0m                   - Show this help\n");
        gfxt_printf("  \x1b[32mclear\x1b[0m                  - Clear screen (Ctrl+L)\n");
        gfxt_printf("  \x1b[32mcolors\x1b[0m                 - Display color test\n");
        gfxt_printf("  \x1b[32mabout\x1b[0m                  - About this terminal\n");
        gfxt_printf("\n");
        gfxt_printf("\x1b[33mEscape sequences for echo -e:\x1b[0m\n");
        gfxt_printf("  \\\\n - newline, \\\\t - tab, \\\\r - carriage return\n");
        gfxt_printf("  \\\\\\\\ - backslash, \\\\e - ESC, \\\\0nnn - octal\n");
        gfxt_printf("\n");
        gfxt_printf("\x1b[33mANSI color codes:\x1b[0m\n");
        gfxt_printf("  \x1b[30mBlack\x1b[0m \x1b[31mRed\x1b[0m \x1b[32mGreen\x1b[0m \x1b[33mYellow\x1b[0m\n");
        gfxt_printf("  \x1b[34mBlue\x1b[0m \x1b[35mMagenta\x1b[0m \x1b[36mCyan\x1b[0m \x1b[37mWhite\x1b[0m\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        // Clear is handled by Ctrl+L, but we can mention it
        gfxt_printf("Use Ctrl+L to clear the screen\n");
    }
    else if (strcmp(cmd, "colors") == 0) {
        gfxt_printf("\x1b[1mColor Test:\x1b[0m\n");
        gfxt_printf("Normal: ");
        for (int i = 30; i <= 37; i++) {
            gfxt_printf("\x1b[%dm%d ", i, i);
        }
        gfxt_printf("\x1b[0m\n");

        gfxt_printf("Bright: ");
        for (int i = 90; i <= 97; i++) {
            gfxt_printf("\x1b[%dm%d ", i, i);
        }
        gfxt_printf("\x1b[0m\n");

        gfxt_printf("\nBackgrounds:\n");
        for (int bg = 40; bg <= 47; bg++) {
            gfxt_printf("\x1b[%dmText ", bg);
            for (int fg = 30; fg <= 37; fg++) {
                gfxt_printf("\x1b[%d;%dmA ", fg, bg);
            }
            gfxt_printf("\x1b[0m\n");
        }
    }
    else if (strcmp(cmd, "about") == 0) {
        gfxt_printf("\x1b[1;36mSimple Terminal v1.0\x1b[0m\n");
        gfxt_printf("A lightweight terminal emulator built with SimpleGFX\n");
        gfxt_printf("\n");
        gfxt_printf("Features:\n");
        gfxt_printf("  • Command history (up/down arrows)\n");
        gfxt_printf("  • Line editing with cursor\n");
        gfxt_printf("  • ANSI color support\n");
        gfxt_printf("  • Escape sequence processing\n");
        gfxt_printf("  • Ctrl+L to clear, Ctrl+C to cancel\n");
        gfxt_printf("\n");
        gfxt_printf("Based on SimpleGFX framework\n");
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        gfxt_printf("Use MENU+X to exit the application\n");
    }
    else {
        gfxt_printf("\x1b[31mCommand not found:\x1b[0m %s\n", cmd);
        gfxt_printf("Type 'help' for available commands\n");
    }
}

const char* get_prompt(void) {
    return "\x1b[32msimplegfx\x1b[0m:\x1b[34m~\x1b[0m$ ";
}

void gfx_app(int init) {
    if (init) {
        // Initialize terminal with 50x20 characters
        gfxt_init(50, 20, execute_command, get_prompt);
        terminal_initialized = 1;

        // Show welcome message
        gfxt_printf("\x1b[1;36mWelcome to Simple Terminal!\x1b[0m\n");
        gfxt_printf("Type 'help' for available commands.\n");
        gfxt_printf("Press \x1b[33mCtrl+L\x1b[0m to clear screen\n\n");
    }
}

void gfx_draw(float fps) {
    if (!terminal_initialized) return;

    // Draw terminal at position (10, 10) with scale 2
    gfxt_draw(10, 10, 2);

    // Show FPS in corner
    gfx_set_color(100, 100, 100);
    char fps_text[32];
    sprintf(fps_text, "%.1f fps", fps);
    gfx_text(fps_text, 2, 2, 1);
}

char last_key = 0;

int gfx_on_key(char key, int down) {
    if (!terminal_initialized) return 0;

    if (!down) {
        last_key = 0;
        return 0;
    }
    if (key == last_key) {
        return 0;
    }

    // Pass key to terminal
    gfxt_on_key(key, 1);

    // Exit on MENU+X
    static int menu_pressed = 0;
    if (key == BTN_MENU) {
        menu_pressed = down ? 1 : 0;
    }
    if (menu_pressed && key == BTN_X && down) {
        return 1;
    }

    last_key = key;
    return 0;
}

void gfx_process_data(int compute_time) {
    // No background processing needed
}
