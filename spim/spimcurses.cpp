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
bool mapped_io;			/* => activate memory-mapped IO */
int pipe_out;
int spim_return_value;		/* Value returned when spim exits */


/* Local variables: */

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

int main () //(int argc, char **argv)
{
    // int i;
    // bool assembly_file_loaded = false;
    // int print_usage_msg = 0;

    console_out.f = stdout;
    message_out.f = stdout;

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
    curses_loop();

    return (spim_return_value);
}


// God help me.
static void curses_loop() {	

    // Wooooo hardcoding file names for the win!
    read_assembly_file("../test/hw_03_q2_c.s");

    static int steps;
    mem_addr addr;
    // bool redo = false;

    addr = PC == 0 ? starting_address() : PC;
    std::vector<std::string> inst_dump = dump_instructions(addr);
    printf("%ld\n", inst_dump.size());
    // return;

    initscr();

    refresh();
    // int i = 0;

    // Reg window will be leftmost and BIG
    WINDOW* reg_win = create_newwin(28, 102, 1, 1);

    int instruction_height = 28;
    int inst_win_y = 1;
    WINDOW* inst_win = create_newwin(instruction_height, 105, inst_win_y, 107);

    int inst_cursor_position = 0;
    int bottom_inst = 0;
  
    // Main loop
    char ch;
    while((ch = getch()) != 'q'){

        refresh();

        // erase();
    
        // A hideous implementation of the "step" code
        // steps = (redo ? steps : get_opt_int ());
        addr = PC == 0 ? starting_address () : PC;

        if (steps == 0)
            steps = 1;
        if (addr != 0)
        {
            bool continuable;
            // console_to_program ();
            if (run_program (addr, 1, false, true, &continuable))
            write_output (message_out, "Breakpoint encountered at 0x%08x\n", PC);
            // console_to_spim ();

            // Dump registers to the screen
            static str_stream ss;
            int hex_flag = 1;
            ss_clear (&ss);
            format_registers (&ss, hex_flag, hex_flag);
            mvwprintw(reg_win, 1, 0, ss_to_string (&ss)); // ez
            box(reg_win, 0 , 0);

            wattron(reg_win, A_BOLD);
            mvwprintw(reg_win, 0,1, "Registers");
            wattroff(reg_win, A_BOLD);

            if (inst_cursor_position + 5 > instruction_height)
            {
                bottom_inst += 5;
                werase(inst_win);
            } else if  (inst_cursor_position < 0)
            {
                bottom_inst -= 5;
                werase(inst_win);
            }

            // Dump instruction list to the screen
            std::string current_inst = inst_to_string (addr);
            mvprintw(0, 108, inst_to_string (addr)); // Print current inst at the top
            for (int i = bottom_inst; i < bottom_inst + instruction_height - 1; i++)
            {
                if ((long unsigned int) i < inst_dump.size())
                {
                    if (current_inst.compare(inst_dump.at(i)) == 0)
                    {
                        wattron(inst_win, A_REVERSE);
                        inst_cursor_position = 1+i-bottom_inst;
                    }
                    mvwprintw(inst_win, 1+i-bottom_inst, 1, inst_dump.at(i).c_str());
                    wattroff(inst_win, A_REVERSE);
                }
            }
            
            box(inst_win, 0 , 0);
            wattron(inst_win, A_BOLD);
            mvwprintw(inst_win, 0,1, "Instructions");
            wattroff(inst_win, A_BOLD);
        }
        
        // char buf[10];
        // snprintf(buf, 10, " %d ",i);
        
        // mvprintw(0, 3, buf);
        wrefresh(reg_win);
        wrefresh(inst_win);

        // i++;
        
        // Exit
        // getch();
    }

    delwin(reg_win);
    delwin(inst_win);
    
    endwin();
}


WINDOW* create_newwin(int height, int width, int starty, int startx)
{	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	box(local_win, 0 , 0);		/* 0, 0 gives default characters 
					 * for the vertical and horizontal
					 * lines			*/
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
  va_list args;

  va_start (args, fmt);

  console_to_spim ();

#ifdef NEED_VFPRINTF
  _doprnt (fmt, args, stderr);
#else
  vfprintf (stderr, fmt, args);
#endif
  va_end (args);
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

