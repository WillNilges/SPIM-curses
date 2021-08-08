/* SPIM S20 MIPS simulator.
   Terminal interface for SPIM simulator.

   Copyright (c) 1990-2015, James R. Larus.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of the James R. Larus nor the names of its contributors may be
   used to endorse or promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <string>
#include <vector>
#include <getopt.h>
#include <fstream>

#include <sstream>
#include <cstdint>
#include <iostream>
#include <iomanip>

#ifdef RS
/* This is problem on HP Snakes, which define RS in syscall.h */
#undef RS
#endif

#include <sys/types.h>
#include <sys/select.h>

#ifdef _AIX
#ifndef NBBY
#define NBBY 8
#endif
#endif

#ifndef WIN32
#include <sys/time.h>
#ifdef NEED_TERMIOS
#include <sys/ioctl.h>
#include <sgtty.h>
#else
#include <termios.h>
#endif
#endif

#include <stdarg.h>

#include "spim.h"
#include "string-stream.h"
#include "spim-utils.h"
#include "inst.h"
#include "reg.h"
#include "mem.h"
#include "parser.h"
#include "sym-tbl.h"
#include "scanner.h"
#include "parser_yacc.h"
#include "data.h"

#include "curses/simpane.h"

using namespace SpimCurses;

/* Internal functions: */

static void console_to_program ();
static void console_to_spim ();
static void control_c_seen (int /*arg*/);
static void curses_loop();
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);
std::vector<std::string> dump_instructions(mem_addr addr);

/* Exported Variables: */

/* Not local, but not export so all files don't need setjmp.h */
jmp_buf spim_top_level_env;	/* For ^C */

bool bare_machine;		/* => simulate bare machine */
bool delayed_branches;		/* => simulate delayed branches */
bool delayed_loads;		/* => simulate delayed loads */
bool accept_pseudo_insts;	/* => parse pseudo instructions  */
bool quiet;			/* => no warning messages */
bool assemble;			/* => assemble, write to stdout and exit */
char *exception_file_name = DEFAULT_EXCEPTION_HANDLER;
port message_out, console_out, console_in;
std::string tmp_console_file, tmp_message_file;
bool mapped_io;			/* => activate memory-mapped IO */
int pipe_out;
int spim_return_value;		/* Value returned when spim exits */

/* Local variables: */

#define WORD_WIDTH_10 10
#define WORD_WIDTH_16 8
#define WORD_WIDTH_DEFAULT 32

// Data window
//
bool st_showUserDataSegment   = true;
bool st_showUserStackSegment  = true;
bool st_showKernelDataSegment = true;
int st_dataSegmentDisplayBase = 16;

// Data segment window
//
std::string displayDataSegments();
std::string formatUserDataSeg();
std::string formatUserStack();
std::string formatKernelDataSeg();
std::string formatMemoryContents(mem_addr from, mem_addr to);
std::string formatPartialQuadWord(mem_addr from, mem_addr to);
std::string formatAsChars(mem_addr from, mem_addr to);
std::string rightJustified(std::string input, int width, char padding);

// TODO: Delete these, also create an object
void show_data_memory(WINDOW* target_window, int start_line, int target_height);
void show_user_stack(WINDOW* target_window, int start_line, int target_height);
void show_registers(WINDOW* target_window, int start_line, int target_height);

// TODO: Roll this into the below function as well.
void show_log(WINDOW* target_window, std::string path, int start_line, int target_height);

// Should reduce code duplication
void show_data(WINDOW* target_window, std::string data, int start_line, int target_height);


// Format SPIM abstractions for display
//
std::string formatAddress(mem_addr addr);
std::string formatWord(mem_word word, int base);
std::string formatChar(int chr);
std::string formatSegLabel(std::string segName, mem_addr low, mem_addr high);
std::string nnbsp(int n);

/* Handy ncurses variables */
int max_row, max_col = 0;

int dat_list = 0;
int be_vert  = 0;

typedef enum PaneContext {
  REGISTERS,
  INSTRUCTIONS,
  DATA,
  STACK,
  OUTPUT,
  LOG,
  INVALID
} PaneContext;

PaneContext context = INSTRUCTIONS;

/* => load standard exception handler */
static bool load_exception_handler = true;
static int console_state_saved;
#ifdef NEED_TERMIOS
static struct sgttyb saved_console_state;
#else
static struct termios saved_console_state;
#endif
static int program_argc;
static char** program_argv;
// static bool dump_user_segments = false;
// static bool dump_all_segments = false;

int main(int argc, char **argv)
{
  /*-------------------------------------------------------------------------
  These variables are used to control the getOpt_long_only command line
  parsing utility.
  --------------------------------------------------------------------------*/
  /* getopt_long stores the option index here. */
  int option_index = 0;
  int rc;
  
  /* Command line parameters */
  int help = 0;
  char *in_file = NULL;
  
  /*-------------------------------------------------------------------------
  add getopt_long parsing code here
  --------------------------------------------------------------------------*/
  
  /* This contains the short command line parameters list   In general
  they SHOULD match the long parameter but DONT HAVE TO
  e.g:  verbose  AND  g    */
  char *getoptOptions = "hf:";
  
  /* This contains the long command line parameter list, it should mostly
  match the short list                                                  */
  struct option long_options[] = {
    /* These options set the same flag. */
    {"help",           no_argument, 0, 'h'},
    
    {"file",    required_argument, 0, 'f'}, 
    
    {0, 0, 0, 0} /* Terminate */
  };


  while ((rc = getopt_long_only(argc, argv, getoptOptions, long_options, &option_index)) != -1){
    switch (rc) {
      case 'f':
        in_file = optarg;
        break;

      case 'h':
        help = 1;
        break;

      case '?':         /* Handle the error cases */
        if (optopt == 'c' || optopt == 'd') {
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        }

        else if (isprint (optopt)) {  /* character is printable */
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        }

        else {         /* Character is NOT printable   */
          fprintf (stderr,  "Unknown option character '%x'.\n", optopt);
        }
        return 1;

      default:       /* oops,  unexpected result */
        fprintf (stderr, "Unexpected result %xX at line %d\n", rc, __LINE__);
        exit(99);
    }
  }
  if (help)
  {
    fprintf(stderr, "You fool, there is no help here.\n");
    exit(1);
  }

    // int i;
    // bool assembly_file_loaded = false;
    // int print_usage_msg = 0;

    // Set up a _very_ cursed alternative to logging to stdout
    char tcf[32] = "/tmp/spimcurses_console_XXXXXX";
    mkstemp(tcf);
    tmp_console_file = tcf;
    console_out.f = fopen(tmp_console_file.c_str(), "w+");

    char tmf[32] = "/tmp/spimcurses_message_XXXXXX";
    mkstemp(tmf);
    tmp_message_file = tmf;
    message_out.f = fopen(tmp_message_file.c_str(), "w+");

    bare_machine = false;
    delayed_branches = false;
    delayed_loads = false;
    accept_pseudo_insts = true;
    quiet = false;
    assemble = false;
    spim_return_value = 0;

    /* Input comes directly (not through stdio): */
    console_in.i = 0;
    mapped_io = false;

    // write_startup_message ();

    if (getenv ("SPIM_EXCEPTION_HANDLER") != NULL)
        exception_file_name = getenv ("SPIM_EXCEPTION_HANDLER");

    initialize_world (load_exception_handler ? exception_file_name : NULL, true);
    initialize_run_stack (program_argc, program_argv);

    // Load in the assembly file you'd like to step through
    read_assembly_file(in_file);

    curses_loop();

    return (spim_return_value);
}

// God help me.
static void curses_loop() {
    static int steps;
    mem_addr addr;
    // bool redo = false;

    addr = PC == 0 ? starting_address() : PC;
    std::vector<std::string> inst_dump = dump_instructions(addr);
    // printf("%ld\n", inst_dump.size()); // Print length of instruction dump

    initscr(); // Init ncurses

    noecho();    // Don't echo input to screen
    curs_set(0); // Don't show terminal cursor
    
    // Get bounds of display
    getmaxyx(stdscr, max_row, max_col);
    
    // == REGISTER PANE ==
    // int reg_win_y = 1;
    // int reg_win_x = 1;
    // int reg_win_height = max_row - 2;
    // int reg_win_width = 29;
    // WINDOW* reg_win = create_newwin(reg_win_height, reg_win_width, reg_win_y, reg_win_x);

    // // int reg_start_x = 0;
    // int reg_start_y = 1;

    SpimCurses::SimPane reg_win("Registers", 1, 1, max_row - 2, 29);

    // == INSTRUCTION PANE ==
    int inst_win_y = 1;
    int inst_win_x = reg_win.get_dims().width + reg_win.get_dims().x;
    int inst_win_height = max_row * 0.8 - 2;
    int inst_win_width = max_col * 0.6; // I want to dedicate half of the remaining screen to the instruction window
    WINDOW* inst_win = create_newwin(inst_win_height, inst_win_width, inst_win_y, inst_win_x);
    scrollok(inst_win, TRUE);
    // scrollok(reg_win, TRUE);

    int inst_start_x = 0;
    int inst_start_y = 0;

    int inst_cursor_position = 0;
    int bottom_inst = 0;
    bool continuable;

    // == DATA PANE ==
    int data_win_y = 1;
    int data_win_x = reg_win.get_dims().width + reg_win.get_dims().x + inst_win_width;
    int data_win_height = max_row * 0.66 - 2;
    int data_win_width = max_col - data_win_x - 1;
    WINDOW* data_win = create_newwin(data_win_height, data_win_width, data_win_y, data_win_x);
    scrollok(data_win, TRUE);

    int dat_list_start = 1;
    // int reg_list_start = 1;


    // == STACK PANE ==
    int stack_win_y = max_row * 0.66 - 1;
    int stack_win_x = reg_win.get_dims().width + reg_win.get_dims().x + inst_win_width;
    int stack_win_height = max_row * 0.33;
    int stack_win_width = max_col - stack_win_x - 1;
    WINDOW* stack_win = create_newwin(stack_win_height, stack_win_width, stack_win_y, stack_win_x);
    scrollok(stack_win, TRUE);

    int stk_list_start = 1;
    // int reg_list_start = 1;


    // == OUTPUT PANE ==
    int output_win_y = max_row * 0.8 - 1;
    int output_win_x = inst_win_x;
    int output_win_height = max_row * 0.2;
    int output_win_width = inst_win_width / 2;
    WINDOW* output_win = create_newwin(output_win_height, output_win_width, output_win_y, output_win_x);
    scrollok(output_win, TRUE);

    int out_list_start = 1;
    // int reg_list_start = 1;


    // == LOG PANE ==
    int log_win_y = max_row * 0.8 - 1;
    int log_win_x = output_win_x + output_win_width;
    int log_win_height = max_row * 0.2;
    int log_win_width = inst_win_width / 2;
    WINDOW* log_win = create_newwin(log_win_height, log_win_width, log_win_y, log_win_x);
    scrollok(log_win, TRUE);

    int log_list_start = 1;
    // int reg_list_start = 1;

    char* begin = "Press any key to begin.";
    attron(A_BOLD);
    mvprintw(max_row/2, max_col/2 - strlen(begin)/2, begin);
    attroff(A_BOLD);
  
    // Main loop
    char ch;
    while((ch = getch()) != 'q'){

        // Clearing by default is terrible and stupid and awful.
        // Shitty hack to "lessen" artifacting.
        // wclear(reg_win);
        wclear(inst_win);
        wclear(data_win);
        wclear(stack_win);
        wclear(log_win);

        int step = 0;
        switch (ch) {
            case 'n':
                step = 1;
                break;
            case 'h':
                // TODO: WTF?
                if (context != DATA && inst_start_x > 0) {
                    inst_start_x--;

                    // Doing this results in small graphical glitches while rapidly
                    // scrolling, but it's necessary to avoid artifacting when scrolling
                    // beyond the window.
                    // wclear(inst_win);
                }
                break;
            case 'j':
                switch (context)
                {
                    case REGISTERS:
                        // reg_start_y--;
                        reg_win.act(SimPane::UP);
                        break;
                    case INSTRUCTIONS:
                        inst_start_y--;
                        break;
                    case DATA:
                        dat_list_start--;
                        break;
                    case STACK:
                        stk_list_start--;
                        break;
                    case OUTPUT:
                        out_list_start--;
                        break;
                    case LOG:
                        log_list_start--;
                        break;
                    case INVALID:
                    default:
                        break;
                }
                break;
            case 'k':
                switch (context)
                {
                    case REGISTERS:
                        // reg_start_y++;
                        reg_win.act(SimPane::DOWN);
                        break;
                    case INSTRUCTIONS:
                        inst_start_y++;
                        break;
                    case DATA:
                        dat_list_start++;
                        break;
                    case STACK:
                        stk_list_start++;
                        break;
                    case OUTPUT:
                        out_list_start++;
                        break;
                    case LOG:
                        log_list_start++;
                        break;
                    case INVALID:
                    default:
                        break;
                }
                break;
            case 'l':
                // TODO: Horizontal scrolling on Data?
                if (context != DATA) {
                    inst_start_x++;
                }
                // wclear(inst_win);
                break;

            // Switch between panes (both c and Shift-C)
            case 'c':
                switch (context)
                {
                    // Look away, kids!
                    case REGISTERS:
                        context = INSTRUCTIONS;
                        break;
                    case INSTRUCTIONS:
                        context = DATA;
                        break;
                    case DATA:
                        context = STACK;
                        break;
                    case STACK:
                        context = OUTPUT;
                        break;
                    case OUTPUT:
                        context = LOG;
                        break;
                    case LOG:
                        context = REGISTERS;
                        break;
                    case INVALID:
                    default:
                        break;
                }
                break;
            case 'C':
                switch (context)
                {
                    // Look away, kids!
                    case REGISTERS:
                        context = LOG;
                        break;
                    case INSTRUCTIONS:
                        context = REGISTERS;
                        break;
                    case DATA:
                        context = INSTRUCTIONS;
                        break;
                    case STACK:
                        context = DATA;
                        break;
                    case OUTPUT:
                        context = STACK;
                        break;
                    case LOG:
                        context = OUTPUT;
                        break;
                    case INVALID:
                    default:
                        break;
                }
                break;
          default:
            refresh();
            break;
        }
    
        // A hideous implementation of the "step" code
        // steps = (redo ? steps : get_opt_int ());
        addr = PC == 0 ? starting_address () : PC;

        // Okay what the actual fuck...
        if (!setjmp (spim_top_level_env)) {
            if (steps == 0)
                steps = 1;
            if (addr != 0)
            {
                char *undefs = undefined_symbol_string ();
                if (undefs != NULL)
                {
                    write_output (message_out, "The following symbols are undefined:\n");
                    write_output (message_out, undefs);
                    write_output (message_out, "\n");
                    free (undefs);
                    
                    // delwin(reg_win);
                    delwin(inst_win);
                    
                    endwin();
                    return;
                }
                console_to_program();
                if(step)
                {
                    if(run_program (addr, 1, false, false, &continuable))
                    {
                        // write_output (message_out, "Breakpoint encountered at 0x%08x\n", PC);

                        // delwin(reg_win);
                        delwin(inst_win);
                        
                        endwin();
                        return;
                    }
                }
                
                // Dump registers to the screen
                // show_registers(reg_win, reg_start_y, inst_win_height);
                static str_stream ss;
                int hex_flag = 1;
                ss_clear (&ss);
                format_registers(&ss, hex_flag, hex_flag);
                reg_win.show_data(ss_to_string(&ss));

                if (inst_cursor_position + 5 > inst_win_height - inst_start_y && ch == 'n')
                    inst_start_y -= 5;
                else if (inst_cursor_position < 0 && ch == 'n')
                    inst_start_y += 5;

                show_data_memory(data_win, dat_list_start, data_win_height);

                show_user_stack(stack_win, stk_list_start, stack_win_height);
                
                // Display instruction list
                // TODO: Why are the lines glitching at the bottom? Is it linewrap?
                // TODO: IDK maybe it's some dumb shit with this: https://www.ibm.com/docs/en/aix/7.2?topic=c-clearok-idlok-leaveok-scrollok-setscrreg-wsetscrreg-subroutine
                std::string current_inst = inst_to_string (addr);
                for (int i = bottom_inst; i < bottom_inst + inst_win_height - 1; i++)
                {
                    if ((long unsigned int) i < inst_dump.size())
                    {
                        if (current_inst.compare(inst_dump.at(i)) == 0)
                        {
                            wattron(inst_win, A_REVERSE);
                            inst_cursor_position = 1+i-bottom_inst;
                        }

                        // To prevent SEGFAULTs while scrolling :)
                        int instruction_start = 0;
                        if (inst_start_x > (int) inst_dump.at(i).length())
                            instruction_start = inst_dump.at(i).length();
                        else
                            instruction_start = inst_start_x;

                        // Print lines
                        mvwprintw(inst_win, 1+i-bottom_inst+inst_start_y, 1, inst_dump.at(i).substr(instruction_start).c_str());
                        wattroff(inst_win, A_REVERSE);
                    }
                }

                show_log(log_win, tmp_message_file, log_list_start, log_win_height);
                show_log(output_win, tmp_console_file, out_list_start, output_win_height);
                
                // Box and label the panes.
                const char* label;

                //TODO?: void label_win(WINDOW* target_window, PaneContext context, char* base_label);
                // box(reg_win, 0 , 0);
                // wattron(reg_win, A_BOLD);
                // label = context == REGISTERS ? "[Registers]" : "Registers";
                // mvwprintw(reg_win, 0,1, label);
                // wattroff(reg_win, A_BOLD);
                reg_win.act(SimPane::BOX);

                box(inst_win, 0, 0);
                wattron(inst_win, A_BOLD);
                label = context == INSTRUCTIONS ? "[Instructions]" : "Instructions";
                mvwprintw(inst_win, 0,1, label);
                wattroff(inst_win, A_BOLD);

                box(data_win, 0, 0);
                wattron(data_win, A_BOLD);
                label = context == DATA ? "[Data Memory]" : "Data Memory";
                mvwprintw(data_win, 0,1, label);
                wattroff(data_win, A_BOLD);

                box(stack_win, 0, 0);
                wattron(stack_win, A_BOLD);
                label = context == STACK ? "[Stack Memory]" : "Stack Memory";
                mvwprintw(stack_win, 0,1, label);
                wattroff(stack_win, A_BOLD);

                box(output_win, 0, 0);
                wattron(output_win, A_BOLD);
                label = context == OUTPUT ? "[Output]" : "Output";
                mvwprintw(output_win, 0,1, label);
                wattroff(output_win, A_BOLD);

                box(log_win, 0, 0);
                wattron(log_win, A_BOLD);
                label = context == LOG ? "[Log]" : "Log";
                mvwprintw(log_win, 0,1, label);
                wattroff(log_win, A_BOLD);

                
                console_to_spim();
            }
        }
        
        // wrefresh(reg_win);
        reg_win.act(SimPane::REFRESH);
        wrefresh(inst_win);
        wrefresh(data_win);
        wrefresh(stack_win);
        wrefresh(output_win);
        wrefresh(log_win);

        mvprintw(max_row - 1, 2, "Press 'N' to advance / Use 'HJKL' to scroll / Press 'C' to switch windows / Press 'Q' to quit");
    }

    // delwin(reg_win);
    delwin(inst_win);
    delwin(data_win);
    delwin(stack_win);
    delwin(output_win);
    delwin(log_win);
    endwin();

    // Clean up log files.
    remove(tmp_console_file.c_str());
    remove(tmp_message_file.c_str());
}

WINDOW* create_newwin(int height, int width, int starty, int startx)
{
    WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	
    // 0, 0 gives default characters for the vertical and horizontal lines
    box(local_win, 0 , 0);
	wrefresh(local_win);		/* Show that box 		*/

	return local_win;
}

std::vector<std::string> dump_instructions(mem_addr addr)
{
    std::vector<std::string> instruction_list;

    str_stream ss;
    instruction *inst;
    std::string str_inst;
    int fake_pc = 0;

    //   std::size_t found;
    do {
        exception_occurred = 0;
        inst = read_mem_inst (addr+fake_pc*4);

        if (exception_occurred)
            {
            error ("Can't print instruction not in text segment (0x%08x)\n", addr);
            // return "";
            }

        ss_init (&ss);
        format_an_inst (&ss, inst, addr+fake_pc*4);
        str_inst = ss_to_string(&ss);
        // printf("%s", ss_to_string(&ss));
        instruction_list.push_back(str_inst);
        fake_pc++;
    } while (str_inst.find("<none>") == std::string::npos);

    return instruction_list;
}

void show_data_memory(WINDOW* target_window, int start_line, int target_height)
{
    int line = start_line;
    std::istringstream mem_map_stream(displayDataSegments());
    while (!mem_map_stream.eof() || line < target_height)
    {
        std::string mem_line;
        getline(mem_map_stream, mem_line);

        // Bold the titles of each section of memory
        if (mem_line.find("Kernel data segment") != std::string::npos ||
            mem_line.find("User Stack")          != std::string::npos ||
            mem_line.find("User data segment")   != std::string::npos)
            wattron(target_window, A_BOLD);
            
        mvwprintw(target_window, line, 1, mem_line.c_str());
        wattroff(target_window, A_BOLD);
        line++;
    }
}

void show_data(WINDOW* target_window, std::string data, int start_line, int target_height)
{
    int line = start_line;
    std::istringstream iss(data);
    while (!iss.eof() || line < target_height)
    {
        std::string current_line;
        getline(iss, current_line);            
        mvwprintw(target_window, line, 1, current_line.c_str());
        wattroff(target_window, A_BOLD);
        line++;
    }
}

void show_user_stack(WINDOW* target_window, int start_line, int target_height)
{
    int line = start_line;
    std::istringstream stack_stream(formatUserStack());
    while (!stack_stream.eof() || line < target_height)
    {
        std::string mem_line;
        getline(stack_stream, mem_line);            
        mvwprintw(target_window, line, 1, mem_line.c_str());
        wattroff(target_window, A_BOLD);
        line++;
    }
}

void show_registers(WINDOW* target_window, int start_line, int target_height)
{
    int line = start_line;
    static str_stream ss;
    int hex_flag = 1;
    ss_clear (&ss);
    format_registers(&ss, hex_flag, hex_flag);
    std::istringstream registers_stream(ss_to_string(&ss));
    while (!registers_stream.eof() || line < target_height)
    {
        std::string mem_line;
        getline(registers_stream, mem_line);            
        mvwprintw(target_window, line, 1, mem_line.c_str());
        line++;
    }
}

void show_log(WINDOW* target_window, std::string path, int start_line, int target_height)
{
    int line = start_line;
    std::string log_line;
    // FILE* f = fopen("Logs.txt", "r");
    std::ifstream is(path.c_str());
    while (std::getline(is, log_line) || line < target_height)
    {
      mvwprintw(target_window, line, 1, log_line.c_str());
      line++;
    }
}

//
// Code for assembling the contents of the data pane.
//

std::string displayDataSegments()
{
    std::string window_contents;
    window_contents += formatUserDataSeg() + formatKernelDataSeg();
    return window_contents;
}


std::string formatUserDataSeg()
{
    if (st_showUserDataSegment)
    {
        return formatSegLabel("User data segment", DATA_BOT, data_top)
            + "\n" + formatMemoryContents(DATA_BOT, data_top);
    }
    else
    {
        return std::string("");
    }
}


std::string formatUserStack()
{
    if (st_showUserStackSegment)
    {
        return formatSegLabel("\nUser Stack", ROUND_DOWN(R[29], BYTES_PER_WORD), STACK_TOP)
            + "\n" + formatMemoryContents(ROUND_DOWN(R[29], BYTES_PER_WORD), STACK_TOP);
    }
    else
    {
        return std::string("");
    }
}


std::string formatKernelDataSeg()
{
    if (st_showKernelDataSegment)
    {
        return formatSegLabel("\nKernel data segment", K_DATA_BOT, k_data_top)
            + "\n" + formatMemoryContents(K_DATA_BOT, k_data_top);
    }
    else
    {
        return std::string("");
    }
}


#define BYTES_PER_LINE (4*BYTES_PER_WORD)


std::string formatMemoryContents(mem_addr from, mem_addr to)
{
    mem_addr i = ROUND_UP(from, BYTES_PER_WORD);
    std::string windowContents = formatPartialQuadWord(i, to);
    i = ROUND_UP(i, BYTES_PER_LINE); // Next quadword

    for ( ; i < to; )
    {
        mem_word val;

        /* Count consecutive zero words */
        int j;
        for (j = 0; (i + (uint32) j * BYTES_PER_WORD) < to; j += 1)
	{
            val = read_mem_word(i + (uint32) j * BYTES_PER_WORD);
            if (val != 0)
	    {
                break;
	    }
	}

        if (j >= 4)
	{
            /* Block of 4 or more zero memory words: */
            windowContents += "[" + formatAddress(i)
                + "]..[" + formatAddress(i + (uint32) j * BYTES_PER_WORD - 1)
                + "]" + nnbsp(2) + "00000000\n";

            i = i + (uint32) j * BYTES_PER_WORD;
            windowContents += formatPartialQuadWord(i, to);
            i = ROUND_UP(i, BYTES_PER_LINE); // Next quadword
	}
        else
	{
            /* Fewer than 4 zero words, print them on a single line: */
            windowContents += "[" + formatAddress(i) + "]" + nnbsp(2);
            mem_addr j = i;
            do
	    {
                val = read_mem_word(i);
                windowContents += nnbsp(2) + formatWord(val, st_dataSegmentDisplayBase);
                i += BYTES_PER_WORD;
	    }
            while ((i % BYTES_PER_LINE) != 0 && i < to);

            windowContents += nnbsp(2) + formatAsChars(j, i) + std::string("\n");
	}
    }
    return windowContents;
}


std::string formatPartialQuadWord(mem_addr from, mem_addr to)
{
    std::string windowContents = std::string("");

    if ((from % BYTES_PER_LINE) != 0 && from < to)
    {
        windowContents += "[" + formatAddress(from) + "]" + nnbsp(2);

        mem_addr a;
        for (a = from; (a % BYTES_PER_LINE) != 0 && from < to; a += BYTES_PER_WORD)
	{
            mem_word val = read_mem_word(a);
            windowContents += nnbsp(2) + formatWord(val, st_dataSegmentDisplayBase);
	}

        windowContents += formatAsChars(from, a) + "\n";
    }

    return windowContents;
}


std::string formatAsChars(mem_addr from, mem_addr to)
{
    std::string windowContents = nnbsp(2);

    if (to - from != BYTES_PER_LINE)
    {
        int missing = (BYTES_PER_LINE - (to - from)) / BYTES_PER_WORD;
        windowContents += nnbsp(2);
        switch (st_dataSegmentDisplayBase)
        {
        case 10: windowContents += nnbsp(missing * (WORD_WIDTH_10 + 2)); break;
        case 16: windowContents += nnbsp(missing * (WORD_WIDTH_16 + 2)); break;
        default: windowContents += nnbsp(missing * (WORD_WIDTH_DEFAULT + 2)); break;
        }
    }

    for (mem_addr a = from; a < to; a += 1)
    {
        mem_word val = read_mem_byte(a);
        windowContents += val + " ";
    }

    return windowContents;
}


//
// Utility functions
//

std::string nnbsp(int n)
{
    std::string str = "";
    int i;
    for (i = 0; i < n; i++)
    {
        str += " ";
    }
    return str;
}


std::string formatAddress(mem_addr addr)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(8) << std::hex << addr;   
    return ss.str();
}

std::string rightJustified(std::string input, int width, char padding)
{
    std::stringstream ss;
    ss << std::setfill(padding) << std::setw(width) << input;   
    return ss.str();
}

std::string formatWord(mem_word word, int base)
{
    int width = 0;
    switch (base)
    {
    case 10: width = WORD_WIDTH_10; break;
    case 16: width = WORD_WIDTH_16; break;
    default: width = WORD_WIDTH_DEFAULT; break;
    }
    // std::string str = std::string::number(word, base);

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(8) << std::hex << word;   
    std::string str = ss.str();
    str.erase(0, str.length() - width); // Negative hex number proceeded by 0xffffffff

    if (str[0] == '-')                   // decimal starting with a negative sign
        return rightJustified(str, width, ' '); // Don't zero pad
    else
        return rightJustified(str, width, '0');
}


// std::string formatChar(int chr)
// {
//     if (chr == ' ')
//     {
//         return std::string("&nbsp;");
//     }
//     else if (chr == '<')
//     {
//         return std::string("&lt;");
//     }
//     else if (chr == '>')
//     {
//         return std::string("&gt;");
//     }
//     else if (chr == '&')
//     {
//         return std::string("&amp;");
//     }
//     else if (chr > ' ' && chr <= '~') // Printable ascii chars
//     {
//         return std::string(QChar(chr));
//     }
//     else
//     {
//         return std::string(QChar('.'));
//     }
// }


std::string formatSegLabel(std::string segName, mem_addr low, mem_addr high)
{
    return segName + " [" + formatAddress(low) + "]..[" + formatAddress(high) + std::string("]");
}


static void
control_c_seen (int /*arg*/)
{
  console_to_spim ();
  write_output (message_out, "\nExecution interrupted\n");
  longjmp (spim_top_level_env, 1);
}


/* SPIM commands */

enum {
  UNKNOWN_CMD = 0,
  EXIT_CMD,
  READ_CMD,
  RUN_CMD,
  STEP_CMD,
  PRINT_CMD,
  PRINT_SYM_CMD,
  PRINT_ALL_REGS_CMD,
  REINITIALIZE_CMD,
  ASM_CMD,
  REDO_CMD,
  NOP_CMD,
  HELP_CMD,
  CONTINUE_CMD,
  SET_BKPT_CMD,
  DELETE_BKPT_CMD,
  LIST_BKPT_CMD,
  DUMPNATIVE_TEXT_CMD,
  DUMP_TEXT_CMD
};

/* Print an error message. */

void
error (char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);

#ifdef NEED_VFPRINTF
  _doprnt (fmt, args, stderr);
#else
  vfprintf (stderr, fmt, args);
#endif
  va_end (args);
}

/* Print the error message then exit. */

void
fatal_error (char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  fmt = va_arg (args, char *);

#ifdef NEED_VFPRINTF
  _doprnt (fmt, args, stderr);
#else
  vfprintf (stderr, fmt, args);
#endif
  exit (-1);
}


/* Print an error message and return to top level. */

void
run_error (char *fmt, ...)
{
    char format[80];
    
    va_list args;

    va_start (args, fmt);

    console_to_spim ();

#ifdef NEED_VFPRINTF
    _doprnt (fmt, args, stderr);
#else
    vsnprintf (format, 80, fmt, args);
#endif
    va_end (args);
    
    // Open a handy error dialogue
    int err_win_y = max_row/2 - 5;
    int err_win_x = max_col/2 - 40;
    int err_win_height = 10;
    int err_win_width = 80;
    WINDOW* error_win = create_newwin(err_win_height, err_win_width, err_win_y, err_win_x);
    wattron(error_win, A_BOLD);
    mvwprintw(error_win, err_win_height/2 - 2, err_win_width/2 - strlen("ERROR ! ! !")/2, "ERROR ! ! !");
    mvwprintw(error_win, err_win_height/2, err_win_width/2 - strlen(format)/2, format);
    refresh();
    wrefresh(error_win);
    attroff(A_BOLD);

    longjmp (spim_top_level_env, 1);
}



/* IO facilities: */

void
write_output (port fp, char *fmt, ...)
{
  va_list args;
  FILE *f;
  int restore_console_to_program = 0;

  va_start (args, fmt);
  f = fp.f;

//   f = fopen("Logs.txt", "w");

  if (console_state_saved)
    {
      restore_console_to_program = 1;
      console_to_spim ();
    }

  if (f != 0)
    {
#ifdef NEED_VFPRINTF
      _doprnt (fmt, args, f);
#else
      vfprintf (f, fmt, args);
#endif
      fprintf(f, "\n");
      fflush (f);
    }
  else
    {
#ifdef NEED_VFPRINTF
      _doprnt (fmt, args, stdout);
#else
      vfprintf (stdout, fmt, args);
#endif
      fflush (stdout);
    }
  va_end (args);

  if (restore_console_to_program)
    console_to_program ();
}


/* Simulate the semantics of fgets (not gets) on Unix file. */

void
read_input (char *str, int str_size)
{
  char *ptr;
  int restore_console_to_program = 0;

  if (console_state_saved)
    {
      restore_console_to_program = 1;
      console_to_spim ();
    }

  ptr = str;

  while (1 < str_size)		/* Reserve space for null */
    {
      char buf[1];
      if (read ((int) console_in.i, buf, 1) <= 0) /* Not in raw mode! */
        break;

      *ptr ++ = buf[0];
      str_size -= 1;

      if (buf[0] == '\n')
    break;
    }

  if (0 < str_size)
    *ptr = '\0';		/* Null terminate input */

  if (restore_console_to_program)
    console_to_program ();
}


/* Give the console to the program for IO. */

static void
console_to_program ()
{
  if (mapped_io && !console_state_saved)
    {
#ifdef NEED_TERMIOS
      int flags;
      ioctl ((int) console_in.i, TIOCGETP, (char *) &saved_console_state);
      flags = saved_console_state.sg_flags;
      saved_console_state.sg_flags = (flags | RAW) & ~(CRMOD|ECHO);
      ioctl ((int) console_in.i, TIOCSETP, (char *) &saved_console_state);
      saved_console_state.sg_flags = flags;
#else
      struct termios params;

      tcgetattr (console_in.i, &saved_console_state);
      params = saved_console_state;
      params.c_iflag &= ~(ISTRIP|INLCR|ICRNL|IGNCR|IXON|IXOFF|INPCK|BRKINT|PARMRK);

      /* Translate CR -> NL to canonicalize input. */
      params.c_iflag |= IGNBRK|IGNPAR|ICRNL;
      params.c_oflag = OPOST|ONLCR;
      params.c_cflag &= ~PARENB;
      params.c_cflag |= CREAD|CS8;
      params.c_lflag = 0;
      params.c_cc[VMIN] = 1;
      params.c_cc[VTIME] = 1;

      tcsetattr (console_in.i, TCSANOW, &params);
#endif
      console_state_saved = 1;
    }
}


/* Return the console to SPIM. */

static void
console_to_spim ()
{
  if (mapped_io && console_state_saved)
#ifdef NEED_TERMIOS
    ioctl ((int) console_in.i, TIOCSETP, (char *) &saved_console_state);
#else
    tcsetattr (console_in.i, TCSANOW, &saved_console_state);
#endif
  console_state_saved = 0;
}


int
console_input_available ()
{
  fd_set fdset;
  struct timeval timeout;

  if (mapped_io)
    {
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      FD_ZERO (&fdset);
      FD_SET ((int) console_in.i, &fdset);
      return (select (sizeof (fdset) * 8, &fdset, NULL, NULL, &timeout));
    }
  else
    return (0);
}


char
get_console_char ()
{
  char buf;

  read ((int) console_in.i, &buf, 1);

  if (buf == 3)			/* ^C */
    control_c_seen (0);
  return (buf);
}


void
put_console_char (char c)
{
  putc (c, console_out.f);
  fflush (console_out.f);
}

