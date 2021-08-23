#pragma once
#include <string>
#include <sstream>
#include <ncurses.h>
#include <fstream>

// Enum to keep track of what pane the user is currently scrolling.
typedef enum PaneContext {
    REGISTERS,
    INSTRUCTIONS,
    DATA,
    STACK,
    OUTPUT,
    LOG,
    INVALID
} PaneContext;


namespace SpimCurses
{
    class CursesPane
    {
        public: 
            CursesPane(/*WINDOW* win, */PaneContext context, int win_height, int win_width, int win_y, int win_x);

            // Render data inside the window
            void show_data(std::string data);
            void show_log(std::string path);

            // Standard ncurses functions
            void draw_box(PaneContext context);
            void refresh();
            void erase();

            // Dimension interfaces
            int gety();
            int getx();
            int geth();
            int getw();
           
            //int sety(int y);
            //int setx(int x);
            //int seth(int height);
            //int setw(int width);

            // Scrolling
            int getrow();
            int getcol();
            void up();
            void down();

            ~CursesPane();

        private:
            WINDOW* win;
            std::string title;
            PaneContext pane_context;
            const int win_height;
            const int win_width;
            const int win_y;
            const int win_x;
            int start_x;
            int start_y;
    };
}
