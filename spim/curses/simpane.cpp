#include <sstream>

#include "simpane.h"

using namespace SpimCurses;

SimPane::SimPane(std::string title, int win_y, int win_x, int win_height, int win_width)
    : title(title), win_y(win_y), win_x(win_x), win_height(win_height), win_width(win_width)
{
    win = newwin(win_height, win_width, start_y, start_x);
	
    // 0, 0 gives default characters for the vertical and horizontal lines
    box(win, 0 , 0);
	wrefresh(win);		/* Show that box 		*/

    scrollok(win, TRUE);

    start_x = 0;
    start_y = 0;
}

void SimPane::show_data(std::string data)
{
    int line = SimPane::start_y;
    std::istringstream iss(data);
    while (!iss.eof() || line < SimPane::win_height)
    {
        std::string current_line;
        getline(iss, current_line);            
        mvwprintw(SimPane::win, line, 1, current_line.c_str());
        wattroff(SimPane::win, A_BOLD);
        line++;
    }
}

void SimPane::act(enum pane_action action)
{
    switch(action)
    {
        case REFRESH:
            wrefresh(SimPane::win);
            break;
        case CLEAR:
            wclear(SimPane::win);
            break;
        case BOX:
            box(SimPane::win, 0, 0);
            wattron(SimPane::win, A_BOLD);
            // label = context == LOG ? std::format("[{}]", title) : title;
            mvwprintw(SimPane::win, 0,1, title.c_str()); // TODO: Change back to label
            wattroff(SimPane::win, A_BOLD);
            break;
        case UP:
            SimPane::start_y--;
            break;
        case DOWN:
            SimPane::start_y++;
            break;
        default:
            break;
    }
}

SimPane::pane_dims SimPane::get_dims()
{
    return { SimPane::win_y, SimPane::win_x, SimPane::win_height, SimPane::win_width };
}

SimPane::~SimPane()
{
    delwin(win);
}