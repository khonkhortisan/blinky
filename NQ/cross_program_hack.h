//This section holds the original includes present in lens.c.
#ifdef _QUAKE_
  #include "bspfile.h"
  #include "client.h"
  #include "cmd.h"
  #include "console.h"
  #include "cvar.h"
  #include "draw.h"
  #include "host.h"
  #include "lens.h"
  #include "mathlib.h"
  #include "quakedef.h"

  #ifndef GLQUAKE
    #include "r_local.h"
  #else
    //why is this greyed out in eclipse? And why can't eclipse simply run make?
    //Why does video options not appear, and then crash if I make it appear?

    //#include "../common/d_init.c"
    //#include "d_iface.h"
    //#include "render.h" //for R_SetVrect, R_ViewChanged which are in r_main.c not gl_rmain.c UGH
	    //Never ever make multiple main... afk, didn't finish the sentence.
    //#include "r_main.c"
    //d_local? glr_local?
    #include "zone.h" //for Z_Malloc, Hunk_TempAlloc
    //or include the below - vidwin.c uses d_local.h, while glvidnt.c uses these
    //#include "draw.h"
    //#include "glquake.h"

	//glReadBuffer
	#include "GL/gl.h"
  #endif

  #include "screen.h"
  #include "sys.h"
  #include "view.h"

//This section holds redefinitions of quake functions for generic non-quake programs.
#else
  //#define GLQUAKE

  //include/mathlib.h
  typedef float vec_t;
  typedef vec_t vec3_t[3];

  //cos
  #include <math.h>


  //include/qtypes.h
  typedef unsigned char byte;
  //NQ/host.c
  byte *host_basepal;
  //undefined - don't use PCX images. (or extract palette)

  //NULL
  #include <cstddef>

  //common/cmd.c
  #define	MAX_ARGS		80
  static int cmd_argc;
  static char *cmd_argv[MAX_ARGS];
  static char *cmd_null_string = "";
  static char *cmd_args = NULL;
  int
  Cmd_Argc(void)
  {
      return cmd_argc;
  }
  char *
  Cmd_Argv(int arg)
  {
      if (arg >= cmd_argc)
  	return cmd_null_string;
      return cmd_argv[arg];
  }

  //NQ/common.c
  float
  Q_atof(const char *str)
  {
      double val;
      int sign;
      int c;
      int decimal, total;

      if (*str == '-') {
  	sign = -1;
  	str++;
      } else
  	sign = 1;

      val = 0;

  //
  // check for hex
  //
      if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
  	str += 2;
  	while (1) {
  	    c = *str++;
  	    if (c >= '0' && c <= '9')
  		val = (val * 16) + c - '0';
  	    else if (c >= 'a' && c <= 'f')
  		val = (val * 16) + c - 'a' + 10;
  	    else if (c >= 'A' && c <= 'F')
  		val = (val * 16) + c - 'A' + 10;
  	    else
  		return val * sign;
  	}
      }
  //
  // check for character
  //
      if (str[0] == '\'') {
  	return sign * str[1];
      }
  //
  // assume decimal
  //
      decimal = -1;
      total = 0;
      while (1) {
  	c = *str++;
  	if (c == '.') {
  	    decimal = total;
  	    continue;
  	}
  	if (c < '0' || c > '9')
  	    break;
  	val = val * 10 + c - '0';
  	total++;
      }

      if (decimal == -1)
  	return val * sign;
      while (total > decimal) {
  	val /= 10;
  	total--;
      }

      return val * sign;
  }

  //include/rb_tree.h
  struct rb_node {
      struct rb_node *rb_parent;
      int rb_color;
  #define	RB_RED		0
  #define	RB_BLACK	1
      struct rb_node *rb_right;
      struct rb_node *rb_left;
  };
  struct rb_root {
      struct rb_node *rb_node;
  };
  #define RB_ROOT	(struct rb_root) { NULL, }

  //include/shell.h
  struct stree_root {
      unsigned int entries;
      unsigned int maxlen;
      unsigned int minlen;
      struct rb_root root;
      struct stree_stack *stack; /* used in STree_ForEach() */
  };
  #define STREE_ROOT (struct stree_root) { 0, 0, -1, RB_ROOT, NULL }

  //Z_Malloc originally from common/zone.c
  #include <stdlib.h>
  void *
  Z_Malloc(int size)
  {
    //allocate on time, instead of from original chunk at start
    return malloc(size);
  }

  //common/shell.c
  void
  STree_AllocInit(void)
  {
    //what is this supposed to do?
  }

  //NQ/common.c
  void
  COM_ScanDir(struct stree_root *root, const char *path, const char *pfx,
  	    const char *ext, bool stripext) //qboolean
  {
    //I don't care about lens filename autocomplete right now.
  }

  //NQ/common.c
  int
  Q_atoi(const char *str)
  {
      int val;
      int sign;
      int c;

      if (*str == '-') {
  	sign = -1;
  	str++;
      } else
  	sign = 1;

      val = 0;

  //
  // check for hex
  //
      if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
  	str += 2;
  	while (1) {
  	    c = *str++;
  	    if (c >= '0' && c <= '9')
  		val = (val << 4) + c - '0';
  	    else if (c >= 'a' && c <= 'f')
  		val = (val << 4) + c - 'a' + 10;
  	    else if (c >= 'A' && c <= 'F')
  		val = (val << 4) + c - 'A' + 10;
  	    else
  		return val * sign;
  	}
      }
  //
  // check for character
  //
      if (str[0] == '\'') {
  	return sign * str[1];
      }
  //#define	MAX_OSPATH	128	// max l  //#define GLQUAKE
  // assume decimal
  //
      while (1) {
  	c = *str++;
  	if (c < '0' || c > '9')
  	    return val * sign;
  	val = val * 10 + c - '0';
      }

      return 0;
  }

  //common/zone.c
  void *
  Hunk_TempAlloc(int size)
  {
    return malloc(size);
  }

  //NQ/common.c
  short (*LittleShort) (short l);

  //common/vid*.c
  void VID_LockBuffer(void) {}
  void VID_UnlockBuffer(void) {}

  #include <stdio.h> //printf

  //O_RDWR
   #include <fcntl.h>

  //umask
  #include <sys/stat.h>

  #include <errno.h>
  //strerror
  #include <string.h>

  //write
  #include <unistd.h>

  //NQ/sys_linux.c
  int
  Sys_FileOpenWrite(const char *path)
  {
      int handle;

      umask(0);

      handle = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);

      if (handle == -1)
  	//Sys_Error("Error opening %s: %s", path, strerror(errno));

      return handle;
  }
  int
  Sys_FileWrite(int handle, const void *src, int count)
  {
      return write(handle, src, count);
  }
  void
  Sys_FileClose(int handle)
  {
      close(handle);
  }

  //NQ/quakedef.h
  	#define	MAX_OSPATH	128	// max length of a filesystem pathname

  //NQ/common.c
  char com_gamedir[MAX_OSPATH] ="./src/bzflag/fisheye/globes";

    //NQ/common.c#define	MAX_OSPATH	128	// max length of a filesystem pathname
    void
    COM_WriteFile(const char *filename, const void *data, int len)
    {
        int handle;
        char name[MAX_OSPATH];

        //sprintf(name, "%s/%s", com_gamedir, filename);
        sprintf(name, "%s", filename);

        handle = Sys_FileOpenWrite(name);
        if (handle == -1) {
  	  //Sys_Printf("COM_WriteFile: failed on %s\n", name);
  	  return;
        }
        Sys_FileWrite(handle, data, len);
        Sys_FileClose(handle);
    }

    //common/mathlib.c
    #define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
    void
    VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc)
    {
        vecc[0] = veca[0] + scale * vecb[0];
        vecc[1] = veca[1] + scale * vecb[1];
        vecc[2] = veca[2] + scale * vecb[2];
    }
    void
    CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
    {
        cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
        cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
        cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
    }
    float
    VectorNormalize(vec3_t v)
    {
        float length, ilength;

        length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
        length = sqrt(length);	// FIXME

        if (length) {
    	ilength = 1 / length;
    	v[0] *= ilength;
    	v[1] *= ilength;
    	v[2] *= ilength;
        }

        return length;

    }



#endif
//This section holds redefinitions of quake functions for specific non-quake programs.

#  if defined _BZFLAG_ //Battlezone Capture the Flag
  #include "ControlPanel.h"	//ControlPanel::addMessage, MessageModes
  #include "playing.h"		//controlPanel

  //common/console.c
  void
  Con_Printf(const char *fmt, ...)
  {
	printf("Con_Printf should go to message box\n");
	controlPanel->addMessage(fmt, -1); //MessageCurrent - bad char* to std::string& conversion?
  }

  //include/cmd.h
  typedef void (*xcommand_t)(void);
  typedef struct stree_root *(*cmd_arg_f)(const char *);
  typedef enum {
      src_client,		/* came in over a net connection as a clc_stringcmd
  			   host_client will be valid during this state. */
      src_command		/* from the command buffer */
  } cmd_source_t;

  //common/cmd.c
  void
  Cmd_AddCommand(const char *cmd_name, xcommand_t function)
  {}
  void
  Cmd_SetCompletion(const char *cmd_name, cmd_arg_f completion)
  {}
  //src_command
  void
  Cmd_ExecuteString(char *text, cmd_source_t src)
  {}

#elif defined _MINETEST_

  //common/console.c
  void
  Con_Printf(const char *fmt, ...)
  {
    printf("Con_Printf should go to chat? or console?");
  }

#elif defined _ADD_YOUR_PROGRAM_HERE_

#endif
