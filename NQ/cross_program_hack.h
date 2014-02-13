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
  #endif

  #include "screen.h"
  #include "sys.h"
  #include "view.h"

//This section holds generic redefinitions of quake functions for non-quake programs.
#else

#endif

//This section contains program-specific quake function redefinitions.
#  if defined _ADD_YOUR_PROGRAM_HERE_

#endif
