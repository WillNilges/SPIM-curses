#pragma once
#include <string>
#include <ncurses.h>

namespace SpimCurses
{
    class SimPane
    {
        public:
            enum pane_action
            {
                REFRESH,
                CLEAR,
                BOX,
                UP,
                DOWN,
                A_FOOL
            };

            struct pane_dims
            {
                int y;
                int x;
                int height;
                int width;
            };

            SimPane(std::string title, int win_y, int win_x, int win_height, int win_width);

            void show_data(std::string data);

            void act(enum pane_action action);

            pane_dims get_dims();

            ~SimPane();

        private:
            WINDOW* win;
            std::string title;
            const int win_y;
            const int win_x;
            const int win_height;
            const int win_width;
            int start_x;
            int start_y;
    };
}