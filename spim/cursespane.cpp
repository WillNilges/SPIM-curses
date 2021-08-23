#include "cursespane.h" // The linter is lying

using namespace SpimCurses;

CursesPane::CursesPane(/*WINDOW* win, */PaneContext context, int win_height, int win_width, int win_y, int win_x)
    : /*win(win),*/ pane_context(context), win_height(win_height), win_width(win_width), win_y(win_y), win_x(win_x) 
{
    win = newwin(win_height, win_width, win_y, win_x);
	
    box(win, 0 , 0);
	wrefresh(win);

    scrollok(win, TRUE);

    start_x = 0;
    start_y = 0;

    switch(context)
    {
        case REGISTERS:
            title = "Registers";
            break;
        case INSTRUCTIONS:
            title = "Instructions";
            break;
        case DATA:
            title = "Data Memory";
            break;
        case STACK:
            title = "Stack Memory";
            break;
        case OUTPUT:
            title = "Output";
            break;
        case LOG: 
            title = "Log";
            break;
        default:
            title = "INVALID";
            break;
    }
}

void CursesPane::show_data(std::string data)
{
    int line = getrow();
    std::istringstream iss(data);
    while (!iss.eof() || line < geth())
    {
        std::string current_line;
        getline(iss, current_line);

        // Bold the titles of stuff we're looking for
        std::string highlight[] = { "Kernel data segment", "User Stack", "User data segment" };
        for (std::string str : highlight)
            if (current_line.find(str) != std::string::npos)
                wattron(win, A_BOLD);

        mvwprintw(win, line, 1, current_line.c_str()); // TODO: Displaying User/Kernel Data is wack. line+1? Damb.
        wattroff(win, A_BOLD);
        line++;
    }
}

void CursesPane::show_log(std::string path)
{
    int line = start_y+1; // Add 1, otherwise the first line will print on the border.
    std::string log_line;
    std::ifstream is(path.c_str());
    while (std::getline(is, log_line) || line < win_height)
    {
      mvwprintw(win, line, 1, log_line.c_str());
      line++;
    }
}

// Standard ncurses functions
void CursesPane::draw_box(PaneContext context)
{
    box(win, 0, 0);
    wattron(win, A_BOLD);
    std::string label;
    label = context == pane_context ? "[" + title + "]" : title;
    mvwprintw(win, 0,1, label.c_str());
//    mvwprintw(win, 0,1, title.c_str()); // TODO: Change back to label 
    wattroff(win, A_BOLD);
}

void CursesPane::refresh()
{
    wrefresh(win);    
}

void CursesPane::erase()
{
    werase(win);
}

// Dimension interfaces
int CursesPane::gety()
{
    return win_y;
}

int CursesPane::getx()
{
    return win_x;
}

int CursesPane::geth()
{
    return win_height;
}
int CursesPane::getw()
{
    return win_width;
}

// Scrolling
int CursesPane::getrow()
{
    return CursesPane::start_y;
}

int CursesPane::getcol()
{
    return CursesPane::start_x;
}

void CursesPane::up()
{
    CursesPane::start_y--;
}
void CursesPane::down()
{
    CursesPane::start_y++;
}

CursesPane::~CursesPane()
{
    delwin(win);
}
