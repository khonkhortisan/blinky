/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// view.c -- player eye positioning

#include "bspfile.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "quakedef.h"
#include "screen.h"
#include "view.h"

/*
 * The view is allowed to move slightly from it's true position for bobbing,
 * but if it exceeds 8 pixels linear distance (spherical, not box), the list
 * of entities sent from the server may not include everything in the pvs,
 * especially when crossing a water boudnary.
 */

// FISHEYE BEGIN EDIT
// ORIGINAL: (N/A)
void R_RenderView_fisheye();

cvar_t ffov = {"ffov", "180", true};
cvar_t fviews = {"fviews", "6", true};
cvar_t fmap = {"fmap", "1", true};
cvar_t fborder = {"fborder", "1", true};
cvar_t fcolor = {"fcolor", "1", true};

// FISHEYE END EDIT

cvar_t scr_ofsx = { "scr_ofsx", "0", false };
cvar_t scr_ofsy = { "scr_ofsy", "0", false };
cvar_t scr_ofsz = { "scr_ofsz", "0", false };

cvar_t cl_rollspeed = { "cl_rollspeed", "200" };

// FISHEYE BEGIN EDIT
// ORIGINAL:
// cvar_t cl_rollangle = { "cl_rollangle", "2.0" };
cvar_t cl_rollangle = { "cl_rollangle", "0.0" };
// FISHEYE END EDIT

cvar_t cl_bob = { "cl_bob", "0.02", false };
cvar_t cl_bobcycle = { "cl_bobcycle", "0.6", false };
cvar_t cl_bobup = { "cl_bobup", "0.5", false };

cvar_t v_kicktime = { "v_kicktime", "0.5", false };

// FISHEYE BEGIN EDIT
// ORIGINAL:
//cvar_t v_kickroll = { "v_kickroll", "0.6", false };
cvar_t v_kickroll = { "v_kickroll", "0.0", false };
// FISHEYE END EDIT

cvar_t v_kickpitch = { "v_kickpitch", "0.6", false };

cvar_t v_iyaw_cycle = { "v_iyaw_cycle", "2", false };
cvar_t v_iroll_cycle = { "v_iroll_cycle", "0.5", false };
cvar_t v_ipitch_cycle = { "v_ipitch_cycle", "1", false };
cvar_t v_iyaw_level = { "v_iyaw_level", "0.3", false };
cvar_t v_iroll_level = { "v_iroll_level", "0.1", false };
cvar_t v_ipitch_level = { "v_ipitch_level", "0.3", false };

cvar_t v_idlescale = { "v_idlescale", "0", false };

cvar_t crosshair = { "crosshair", "0", true };
cvar_t crosshaircolor = { "crosshaircolor", "79", true };
cvar_t cl_crossx = { "cl_crossx", "0", false };
cvar_t cl_crossy = { "cl_crossy", "0", false };

#ifdef GLQUAKE
cvar_t gl_cshiftpercent = { "gl_cshiftpercent", "100", false };
#endif

float v_dmg_time, v_dmg_roll, v_dmg_pitch;

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t forward, right, up;

float
V_CalcRoll(vec3_t angles, vec3_t velocity)
{
    float sign;
    float side;
    float value;

    AngleVectors(angles, forward, right, up);
    side = DotProduct(velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabs(side);

    value = cl_rollangle.value;
//      if (cl.inwater)
//              value *= 6;

    if (side < cl_rollspeed.value)
	side = side * value / cl_rollspeed.value;
    else
	side = value;

    return side * sign;

}


/*
===============
V_CalcBob

===============
*/
float
V_CalcBob(void)
{
    float bob;
    float cycle;

    /* Avoid divide-by-zero, don't bob */
    if (!cl_bobcycle.value)
	return 0.0f;

    cycle = cl.time - (int)(cl.time / cl_bobcycle.value) * cl_bobcycle.value;
    cycle /= cl_bobcycle.value;
    if (cycle < cl_bobup.value)
	cycle = M_PI * cycle / cl_bobup.value;
    else
	cycle =
	    M_PI + M_PI * (cycle - cl_bobup.value) / (1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

    bob =
	sqrt(cl.velocity[0] * cl.velocity[0] +
	     cl.velocity[1] * cl.velocity[1]) * cl_bob.value;
//Con_Printf ("speed: %5.1f\n", Length(cl.velocity));
    bob = bob * 0.3 + bob * 0.7 * sin(cycle);
    if (bob > 4)
	bob = 4;
    else if (bob < -7)
	bob = -7;

    return bob;
}


//=============================================================================


cvar_t v_centermove = { "v_centermove", "0.15", false };
cvar_t v_centerspeed = { "v_centerspeed", "500" };


void
V_StartPitchDrift(void)
{
#if 1
    if (cl.laststop == cl.time) {
	return;			// something else is keeping it from drifting
    }
#endif
    if (cl.nodrift || !cl.pitchvel) {
	cl.pitchvel = v_centerspeed.value;
	cl.nodrift = false;
	cl.driftmove = 0;
    }
}

void
V_StopPitchDrift(void)
{
    cl.laststop = cl.time;
    cl.nodrift = true;
    cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void
V_DriftPitch(void)
{
    float delta, move;

    if (noclip_anglehack || !cl.onground || cls.demoplayback) {
	cl.driftmove = 0;
	cl.pitchvel = 0;
	return;
    }
// don't count small mouse motion
    if (cl.nodrift) {
	if (fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
	    cl.driftmove = 0;
	else
	    cl.driftmove += host_frametime;

	if (cl.driftmove > v_centermove.value) {
	    V_StartPitchDrift();
	}
	return;
    }

    delta = cl.idealpitch - cl.viewangles[PITCH];

    if (!delta) {
	cl.pitchvel = 0;
	return;
    }

    move = host_frametime * cl.pitchvel;
    cl.pitchvel += host_frametime * v_centerspeed.value;

    if (delta > 0) {
	if (move > delta) {
	    cl.pitchvel = 0;
	    move = delta;
	}
	cl.viewangles[PITCH] += move;
    } else if (delta < 0) {
	if (move > -delta) {
	    cl.pitchvel = 0;
	    move = -delta;
	}
	cl.viewangles[PITCH] -= move;
    }
}





/*
==============================================================================

				PALETTE FLASHES

==============================================================================
*/


cshift_t cshift_empty = { {130, 80, 50}, 0 };
cshift_t cshift_water = { {130, 80, 50}, 128 };
cshift_t cshift_slime = { {0, 25, 5}, 150 };
cshift_t cshift_lava = { {255, 80, 0}, 150 };

cvar_t v_gamma = { "gamma", "1", true };

byte gammatable[256];		// palette is sent through this

#ifdef	GLQUAKE
unsigned short ramps[3][256];
float v_blend[4];		// rgba 0.0 - 1.0
#endif

void
BuildGammaTable(float g)
{
    int i, inf;

    if (g == 1.0) {
	for (i = 0; i < 256; i++)
	    gammatable[i] = i;
	return;
    }

    for (i = 0; i < 256; i++) {
	inf = 255 * pow((i + 0.5) / 255.5, g) + 0.5;
	if (inf < 0)
	    inf = 0;
	if (inf > 255)
	    inf = 255;
	gammatable[i] = inf;
    }
}

/*
=================
V_CheckGamma
=================
*/
qboolean
V_CheckGamma(void)
{
    static float oldgammavalue;

    if (v_gamma.value == oldgammavalue)
	return false;
    oldgammavalue = v_gamma.value;

    BuildGammaTable(v_gamma.value);
    vid.recalc_refdef = 1;	// force a surface cache flush

    return true;
}



/*
===============
V_ParseDamage
===============
*/
void
V_ParseDamage(void)
{
    int armor, blood;
    vec3_t from;
    int i;
    vec3_t forward, right, up;
    entity_t *ent;
    float side;
    float count;

    armor = MSG_ReadByte();
    blood = MSG_ReadByte();
    for (i = 0; i < 3; i++)
	from[i] = MSG_ReadCoord();

    count = blood * 0.5 + armor * 0.5;
    if (count < 10)
	count = 10;

    cl.faceanimtime = cl.time + 0.2;	// but sbar face into pain frame

    cl.cshifts[CSHIFT_DAMAGE].percent += 3 * count;
    if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
	cl.cshifts[CSHIFT_DAMAGE].percent = 0;
    if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
	cl.cshifts[CSHIFT_DAMAGE].percent = 150;

    if (armor > blood) {
	cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
    } else if (armor) {
	cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
    } else {
	cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
	cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
    }

//
// calculate view angle kicks
//
    ent = &cl_entities[cl.viewentity];

    VectorSubtract(from, ent->origin, from);
    VectorNormalize(from);

    AngleVectors(ent->angles, forward, right, up);

    side = DotProduct(from, right);
    v_dmg_roll = count * side * v_kickroll.value;

    side = DotProduct(from, forward);
    v_dmg_pitch = count * side * v_kickpitch.value;

    v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void
V_cshift_f(void)
{
    cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
    cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
    cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
    cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void
V_BonusFlash_f(void)
{
    cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
    cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
    cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
    cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void
V_SetContentsColor(int contents)
{
    switch (contents) {
    case CONTENTS_EMPTY:
    case CONTENTS_SOLID:
	cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
	break;
    case CONTENTS_LAVA:
	cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
	break;
    case CONTENTS_SLIME:
	cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
	break;
    default:
	cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
    }
}

/*
=============
V_CalcPowerupCshift
=============
*/
void
V_CalcPowerupCshift(void)
{
    if (cl.items & IT_QUAD) {
	cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
	cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
	cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
	cl.cshifts[CSHIFT_POWERUP].percent = 30;
    } else if (cl.items & IT_SUIT) {
	cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
	cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
	cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
	cl.cshifts[CSHIFT_POWERUP].percent = 20;
    } else if (cl.items & IT_INVISIBILITY) {
	cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
	cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
	cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
	cl.cshifts[CSHIFT_POWERUP].percent = 100;
    } else if (cl.items & IT_INVULNERABILITY) {
	cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
	cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
	cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
	cl.cshifts[CSHIFT_POWERUP].percent = 30;
    } else
	cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

/*
=============
V_CalcBlend
=============
*/
#ifdef	GLQUAKE
void
V_CalcBlend(void)
{
    float r, g, b, a, a2;
    int j;

    r = g = b = a = 0;

    if (gl_cshiftpercent.value) {
	for (j = 0; j < NUM_CSHIFTS; j++) {
	    a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) /
		255.0;
	    if (!a2)
		continue;
	    a = a + a2 * (1 - a);
	    a2 = a2 / a;
	    r = r * (1 - a2) + cl.cshifts[j].destcolor[0] * a2;
	    g = g * (1 - a2) + cl.cshifts[j].destcolor[1] * a2;
	    b = b * (1 - a2) + cl.cshifts[j].destcolor[2] * a2;
	}
    }

    v_blend[0] = r / 255.0;
    v_blend[1] = g / 255.0;
    v_blend[2] = b / 255.0;
    v_blend[3] = a;
    if (v_blend[3] > 1)
	v_blend[3] = 1;
    if (v_blend[3] < 0)
	v_blend[3] = 0;
}
#endif

/*
=============
V_UpdatePalette
=============
*/
#ifdef	GLQUAKE
void
V_UpdatePalette(void)
{
    int i;
    qboolean force;

    V_CalcPowerupCshift();

    /* drop the damage value */
    cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
    if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
	cl.cshifts[CSHIFT_DAMAGE].percent = 0;

    /* drop the bonus value */
    cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
    if (cl.cshifts[CSHIFT_BONUS].percent < 0)
	cl.cshifts[CSHIFT_BONUS].percent = 0;

    force = V_CheckGamma();

    if (force) {
	for (i = 0; i < 256; i++) {
	    ramps[0][i] = gammatable[i] << 8;
	    ramps[1][i] = gammatable[i] << 8;
	    ramps[2][i] = gammatable[i] << 8;
	}
	if (VID_IsFullScreen() && VID_SetGammaRamp)
	    VID_SetGammaRamp(ramps);
	//VID_ShiftPalette(NULL);
    }
}
#else // !GLQUAKE
void
V_UpdatePalette(void)
{
    int i, j;
    qboolean new;
    byte *basepal, *newpal;
    byte pal[768];
    int r, g, b;
    qboolean force;

    V_CalcPowerupCshift();

    new = false;

    for (i = 0; i < NUM_CSHIFTS; i++) {
	if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent) {
	    new = true;
	    cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
	}
	for (j = 0; j < 3; j++)
	    if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j]) {
		new = true;
		cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
	    }
    }

// drop the damage value
    cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
    if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
	cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
    cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
    if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
	cl.cshifts[CSHIFT_BONUS].percent = 0;

    force = V_CheckGamma();
    if (!new && !force)
	return;

    basepal = host_basepal;
    newpal = pal;

    for (i = 0; i < 256; i++) {
	r = basepal[0];
	g = basepal[1];
	b = basepal[2];
	basepal += 3;

	for (j = 0; j < NUM_CSHIFTS; j++) {
	    r += (cl.cshifts[j].percent *
		  (cl.cshifts[j].destcolor[0] - r)) >> 8;
	    g += (cl.cshifts[j].percent *
		  (cl.cshifts[j].destcolor[1] - g)) >> 8;
	    b += (cl.cshifts[j].percent *
		  (cl.cshifts[j].destcolor[2] - b)) >> 8;
	}

	newpal[0] = gammatable[r];
	newpal[1] = gammatable[g];
	newpal[2] = gammatable[b];
	newpal += 3;
    }

    VID_ShiftPalette(pal);
}
#endif // !GLQUAKE

/*
==============================================================================

				VIEW RENDERING

==============================================================================
*/

float
angledelta(float a)
{
    a = anglemod(a);
    if (a > 180)
	a -= 360;
    return a;
}

/*
==================
CalcGunAngle
==================
*/
void
CalcGunAngle(void)
{
    float yaw, pitch, move;
    static float oldyaw = 0;
    static float oldpitch = 0;

    yaw = r_refdef.viewangles[YAW];
    pitch = -r_refdef.viewangles[PITCH];

    yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
    if (yaw > 10)
	yaw = 10;
    if (yaw < -10)
	yaw = -10;
    pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
    if (pitch > 10)
	pitch = 10;
    if (pitch < -10)
	pitch = -10;
    move = host_frametime * 20;
    if (yaw > oldyaw) {
	if (oldyaw + move < yaw)
	    yaw = oldyaw + move;
    } else {
	if (oldyaw - move > yaw)
	    yaw = oldyaw - move;
    }

    if (pitch > oldpitch) {
	if (oldpitch + move < pitch)
	    pitch = oldpitch + move;
    } else {
	if (oldpitch - move > pitch)
	    pitch = oldpitch - move;
    }

    oldyaw = yaw;
    oldpitch = pitch;

    cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
    cl.viewent.angles[PITCH] = -(r_refdef.viewangles[PITCH] + pitch);

    cl.viewent.angles[ROLL] -=
	v_idlescale.value * sin(cl.time * v_iroll_cycle.value) *
	v_iroll_level.value;
    cl.viewent.angles[PITCH] -=
	v_idlescale.value * sin(cl.time * v_ipitch_cycle.value) *
	v_ipitch_level.value;
    cl.viewent.angles[YAW] -=
	v_idlescale.value * sin(cl.time * v_iyaw_cycle.value) *
	v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void
V_BoundOffsets(void)
{
    entity_t *ent;

    ent = &cl_entities[cl.viewentity];

// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

    if (r_refdef.vieworg[0] < ent->origin[0] - 14)
	r_refdef.vieworg[0] = ent->origin[0] - 14;
    else if (r_refdef.vieworg[0] > ent->origin[0] + 14)
	r_refdef.vieworg[0] = ent->origin[0] + 14;
    if (r_refdef.vieworg[1] < ent->origin[1] - 14)
	r_refdef.vieworg[1] = ent->origin[1] - 14;
    else if (r_refdef.vieworg[1] > ent->origin[1] + 14)
	r_refdef.vieworg[1] = ent->origin[1] + 14;
    if (r_refdef.vieworg[2] < ent->origin[2] - 22)
	r_refdef.vieworg[2] = ent->origin[2] - 22;
    else if (r_refdef.vieworg[2] > ent->origin[2] + 30)
	r_refdef.vieworg[2] = ent->origin[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void
V_AddIdle(void)
{
    r_refdef.viewangles[ROLL] +=
	v_idlescale.value * sin(cl.time * v_iroll_cycle.value) *
	v_iroll_level.value;
    r_refdef.viewangles[PITCH] +=
	v_idlescale.value * sin(cl.time * v_ipitch_cycle.value) *
	v_ipitch_level.value;
    r_refdef.viewangles[YAW] +=
	v_idlescale.value * sin(cl.time * v_iyaw_cycle.value) *
	v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void
V_CalcViewRoll(void)
{
    float side;

    side = V_CalcRoll(cl_entities[cl.viewentity].angles, cl.velocity);
    r_refdef.viewangles[ROLL] += side;

    if (v_dmg_time > 0) {
	r_refdef.viewangles[ROLL] +=
	    v_dmg_time / v_kicktime.value * v_dmg_roll;
	r_refdef.viewangles[PITCH] +=
	    v_dmg_time / v_kicktime.value * v_dmg_pitch;
	v_dmg_time -= host_frametime;
    }

    if (cl.stats[STAT_HEALTH] <= 0) {
	r_refdef.viewangles[ROLL] = 80;	// dead view angle
	return;
    }

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void
V_CalcIntermissionRefdef(void)
{
    entity_t *ent, *view;
    float old;

// ent is the player model (visible when out of body)
    ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
    view = &cl.viewent;

    VectorCopy(ent->origin, r_refdef.vieworg);
    VectorCopy(ent->angles, r_refdef.viewangles);
    view->model = NULL;

// allways idle in intermission
    old = v_idlescale.value;
    v_idlescale.value = 1;
    V_AddIdle();
    v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
void
V_CalcRefdef(void)
{
    entity_t *ent, *view;
    int i;
    vec3_t forward, right, up;
    vec3_t angles;
    float bob;
    static float oldz = 0;

    V_DriftPitch();

// ent is the player model (visible when out of body)
    ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
    view = &cl.viewent;

// transform the view offset by the model's matrix to get the offset from
// model origin for the view
    ent->angles[YAW] = cl.viewangles[YAW];	// the model should face
    // the view dir
    ent->angles[PITCH] = -cl.viewangles[PITCH];	// the model should face
    // the view dir

    bob = V_CalcBob();

// refresh position
    VectorCopy(ent->origin, r_refdef.vieworg);
    r_refdef.vieworg[2] += cl.viewheight + bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
    r_refdef.vieworg[0] += 1.0 / 32;
    r_refdef.vieworg[1] += 1.0 / 32;
    r_refdef.vieworg[2] += 1.0 / 32;

    VectorCopy(cl.viewangles, r_refdef.viewangles);
    V_CalcViewRoll();
    V_AddIdle();

// offsets
    angles[PITCH] = -ent->angles[PITCH];	// because entity pitches are
    //  actually backward
    angles[YAW] = ent->angles[YAW];
    angles[ROLL] = ent->angles[ROLL];

    AngleVectors(angles, forward, right, up);

    for (i = 0; i < 3; i++)
	r_refdef.vieworg[i] += scr_ofsx.value * forward[i]
	    + scr_ofsy.value * right[i]
	    + scr_ofsz.value * up[i];

    V_BoundOffsets();

// set up gun position
    VectorCopy(cl.viewangles, view->angles);

    CalcGunAngle();

    VectorCopy(ent->origin, view->origin);
    view->origin[2] += cl.viewheight;

    for (i = 0; i < 3; i++) {
	view->origin[i] += forward[i] * bob * 0.4;
//              view->origin[i] += right[i]*bob*0.4;
//              view->origin[i] += up[i]*bob*0.8;
    }
    view->origin[2] += bob;

// fudge position around to keep amount of weapon visible
// roughly equal with different FOV

#if 0
    if (cl.model_precache[cl.stats[STAT_WEAPON]]
	&& strcmp(cl.model_precache[cl.stats[STAT_WEAPON]]->name,
		  "progs/v_shot2.mdl"))
#endif
	if (scr_viewsize.value == 110)
	    view->origin[2] += 1;
	else if (scr_viewsize.value == 100)
	    view->origin[2] += 2;
	else if (scr_viewsize.value == 90)
	    view->origin[2] += 1;
	else if (scr_viewsize.value == 80)
	    view->origin[2] += 0.5;

    view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
    view->frame = cl.stats[STAT_WEAPONFRAME];
    view->colormap = vid.colormap;

// set up the refresh position
    VectorAdd(r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);

// smooth out stair step ups
    if (cl.onground && ent->origin[2] - oldz > 0) {
	float steptime;

	steptime = cl.time - cl.oldtime;
	if (steptime < 0)
//FIXME         I_Error ("steptime < 0");
	    steptime = 0;

	oldz += steptime * 80;
	if (oldz > ent->origin[2])
	    oldz = ent->origin[2];
	if (ent->origin[2] - oldz > 12)
	    oldz = ent->origin[2] - 12;
	r_refdef.vieworg[2] += oldz - ent->origin[2];
	view->origin[2] += oldz - ent->origin[2];
    } else
	oldz = ent->origin[2];

    if (chase_active.value)
	Chase_Update();
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void
V_RenderView(void)
{
    if (con_forcedup)
	return;

// don't allow cheats in multiplayer
    if (cl.maxclients > 1) {
	Cvar_Set("scr_ofsx", "0");
	Cvar_Set("scr_ofsy", "0");
	Cvar_Set("scr_ofsz", "0");
    }

    if (cl.intermission) {	// intermission / finale rendering
	V_CalcIntermissionRefdef();
    } else {
	if (!cl.paused /* && (sv.maxclients > 1 || key_dest == key_game) */ )
	    V_CalcRefdef();
    }

    // FISHEYE BEGIN EDIT
    // ORIGINAL:
    // R_PushDlights();
    // R_RenderView();
    R_RenderView_fisheye();
    // FISHEYE END EDIT

#ifndef GLQUAKE
    if (crosshair.value)
	Draw_Crosshair();
#endif

}

//============================================================================

/*
=============
V_Init
=============
*/
void
V_Init(void)
{
    Cmd_AddCommand("v_cshift", V_cshift_f);
    Cmd_AddCommand("bf", V_BonusFlash_f);
    Cmd_AddCommand("centerview", V_StartPitchDrift);

    Cvar_RegisterVariable(&v_centermove);
    Cvar_RegisterVariable(&v_centerspeed);

    Cvar_RegisterVariable(&v_iyaw_cycle);
    Cvar_RegisterVariable(&v_iroll_cycle);
    Cvar_RegisterVariable(&v_ipitch_cycle);
    Cvar_RegisterVariable(&v_iyaw_level);
    Cvar_RegisterVariable(&v_iroll_level);
    Cvar_RegisterVariable(&v_ipitch_level);

    Cvar_RegisterVariable(&v_idlescale);
    Cvar_RegisterVariable(&crosshair);
    Cvar_RegisterVariable(&crosshaircolor);
    Cvar_RegisterVariable(&cl_crossx);
    Cvar_RegisterVariable(&cl_crossy);
#ifdef GLQUAKE
    Cvar_RegisterVariable(&gl_cshiftpercent);
#endif

    Cvar_RegisterVariable(&scr_ofsx);
    Cvar_RegisterVariable(&scr_ofsy);
    Cvar_RegisterVariable(&scr_ofsz);
    Cvar_RegisterVariable(&cl_rollspeed);
    Cvar_RegisterVariable(&cl_rollangle);
    Cvar_RegisterVariable(&cl_bob);
    Cvar_RegisterVariable(&cl_bobcycle);
    Cvar_RegisterVariable(&cl_bobup);

    Cvar_RegisterVariable(&v_kicktime);
    Cvar_RegisterVariable(&v_kickroll);
    Cvar_RegisterVariable(&v_kickpitch);

    BuildGammaTable(1.0);	// no gamma yet
    Cvar_RegisterVariable(&v_gamma);

    // FISHEYE BEGIN EDIT
    // ORIGINAL: (N/A)
	 Cvar_RegisterVariable (&ffov);
	 Cvar_RegisterVariable (&fviews);
    Cvar_RegisterVariable (&fmap);
    Cvar_RegisterVariable (&fborder);
    Cvar_RegisterVariable (&fcolor);
    // FISHEYE END EDIT
}

// FISHEYE BEGIN EDIT
// ORIGINAL: (N/A)

typedef unsigned char B;

#define BOX_FRONT  0
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_RIGHT  1
#define BOX_TOP    4
#define BOX_BOTTOM 5

#define PI 3.141592654

#define DEG(x) (x / PI * 180.0)
#define RAD(x) (x * PI / 180.0)

struct my_coords
	{
	double x, y, z;
	};

struct my_angles
	{
	double yaw, pitch, roll;
	};

void x_rot(struct my_coords *c, double pitch);
void y_rot(struct my_coords *c, double yaw);
void z_rot(struct my_coords *c, double roll);
void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a);

// get_ypr()

void get_ypr(double yaw, double pitch, double roll, int side, struct my_angles *a)
  {
  struct my_coords o, u;

  // get 'o' (observer) and 'u' ('this_way_up') depending on box side

  switch(side)
    {
    case BOX_FRONT:
      //printf("(FRONT)");
      o.x =  0.0; o.y =  0.0; o.z =  1.0;
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_BEHIND:
      //printf("(BEHIND)");
      o.x =  0.0; o.y =  0.0; o.z = -1.0;
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_LEFT:
      //printf("(LEFT)");
      o.x = -1.0; o.y =  0.0; o.z =  0.0;
      u.x = -1.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_RIGHT:
      //printf("(RIGHT)");
      o.x =  1.0; o.y =  0.0; o.z =  0.0;
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_TOP:
      //printf("(TOP)");
      o.x =  0.0; o.y = -1.0; o.z =  0.0;
      u.x =  0.0; u.y =  0.0; u.z = -1.0; break;
    case BOX_BOTTOM:
      //printf("(BOTTOM)");
      o.x =  0.0; o.y =  1.0; o.z =  0.0;
      u.x =  0.0; u.y =  0.0; u.z = -1.0; break;
    }

  //printf(" - [inputs: yaw = %.4f, pitch = %.4f, roll = %.4f]\n", yaw, pitch, roll);

  z_rot(&o, roll); z_rot(&u, roll);
  x_rot(&o, pitch); x_rot(&u, pitch);
  y_rot(&o, yaw); y_rot(&u, yaw);

  my_get_angles(&o, &u, a);

  /* normalise angles */

  while (a->yaw   <   0.0) a->yaw   += 360.0;
  while (a->yaw   > 360.0) a->yaw   -= 360.0;
  while (a->pitch <   0.0) a->pitch += 360.0;
  while (a->pitch > 360.0) a->pitch -= 360.0;
  while (a->roll  <   0.0) a->roll  += 360.0;
  while (a->roll  > 360.0) a->roll  -= 360.0;

  //printf("get_ypr -> %.4f, %.4f, %.4f\n", a->yaw, a->pitch, a->roll);
  }

/* my_get_angles */

void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a)
  {
  double rad_yaw, rad_pitch;
  struct my_coords o, u;

  a->pitch = 0.0;
  a->yaw = 0.0;
  a->roll = 0.0;

  // make a copy of the coords

  o.x = in_o->x; o.y = in_o->y; o.z = in_o->z;
  u.x = in_u->x; u.y = in_u->y; u.z = in_u->z;

  //printf("%.4f, %.4f, %.4f - \n", o.x, o.y, o.z);

  // special case when looking straight up or down

  if ((o.x == 0.0) && (o.z == 0.0))
    {
    // printf("special!\n");
    a->yaw   = 0.0;
    if (o.y > 0.0) { a->pitch = -90.0; a->roll = 180.0 - DEG(atan2(u.x, u.z)); } // down
    else           { a->pitch =  90.0; a->roll = DEG(atan2(u.x, u.z)); } // up
    return;
    }

/******************************************************************************/

  // get yaw angle and then rotate o and u so that yaw = 0

  rad_yaw = atan2(-o.x, o.z);
  a->yaw  = DEG(rad_yaw);

  y_rot(&o, -rad_yaw);
  y_rot(&u, -rad_yaw);

  //printf("%.4f, %.4f, %.4f - stage 1\n", o.x, o.y, o.z);

  // get pitch and then rotate o and u so that pitch = 0

  rad_pitch = atan2(-o.y, o.z);
  a->pitch  = DEG(rad_pitch);

  x_rot(&o, -rad_pitch);
  x_rot(&u, -rad_pitch);

  //printf("%.4f, %.4f, %.4f - stage 2\n", u.x, u.y, u.z);

  // get roll

  a->roll = DEG(-atan2(u.x, u.y));

  //printf("yaw = %.4f, pitch = %.4f, roll = %.4f\n", a->yaw, a->pitch, a->roll);
  }

/*******************************************************************************/

/* x_rot (pitch) */

void x_rot(struct my_coords *c, double pitch)
	{
	double nx, ny, nz;

	nx = c->x;
	ny = (c->y * cos(pitch)) - (c->z * sin(pitch));
	nz = (c->y * sin(pitch)) + (c->z * cos(pitch));

	c->x = nx; c->y = ny; c->z = nz;

	/*printf("x_rot: %.4f, %.4f, %.4f\n", c->x, c->y, c->z);*/
	}

/* y_rot (yaw) */

void y_rot(struct my_coords *c, double yaw)
	{
	double nx, ny, nz;

	nx = (c->x * cos(yaw)) - (c->z * sin(yaw));
	ny = c->y;
	nz = (c->x * sin(yaw)) + (c->z * cos(yaw));

	c->x = nx; c->y = ny; c->z = nz;
	}

/* z_rot (roll) */

void z_rot(struct my_coords *c, double roll)
	{
	double nx, ny, nz;

	nx = (c->x * cos(roll)) - (c->y * sin(roll));
	ny = (c->x * sin(roll)) + (c->y * cos(roll));
	nz = c->z;

	c->x = nx; c->y = ny; c->z = nz;
	}

void rendercopy(int *dest) {
  int *p = (int*)vid.buffer;
  int pad = 5;
  int x, y;
  int color = (int)fcolor.value;
  R_PushDlights();
  R_RenderView();
  int border = (int)fborder.value;
  for(y = 0;y<vid.height;y++) {
    for(x = 0;x<(vid.width/4);x++,dest++) {
      int isborder = y<=pad || y+pad >= vid.height || x <= pad || x+pad>=vid.width/4;
      *dest = (border && isborder) ? color : p[x];
    } 

    p += (vid.rowbytes/4);
  };
};

void renderlookup(B **offs, B* bufs) {
  B *p = (B*)vid.buffer;
  int x, y;
  for(y = 0;y<vid.height;y++) {
    for(x = 0;x<vid.width;x++,offs++) p[x] = **offs;
    p += vid.rowbytes;
  };
};

void fisheyeMap(int width, int height, double fov, double dx, double dy, double *sx, double *sy, double *sz)
{
    double yaw = sqrt(dx*dx+dy*dy)*fov/((double)height);
    double roll = -atan2(dy,dx);

    *sx = sin(yaw) * cos(roll);
    *sy = sin(yaw) * sin(roll);
    *sz = cos(yaw);
}

void cylinderMap(int width, int height, double fov, double dx, double dy, double *sx, double *sy, double *sz)
{
    // create forward vector
    *sx = 0;
    *sy = 0;
    *sz = 1;

    double az = dx*fov/(double)height;
    double el = -dy*fov/(double)height;

    *sx = sin(az)*cos(el);
    *sy = sin(el);
    *sz = cos(az)*cos(el);
}

void perspectiveMap(int width, int height, double fov, double dx, double dy, double *sx, double *sy, double *sz)
{
    // create forward vector
    *sx = 0;
    *sy = 0;
    *sz = 1;

    double a = (double)height/2/tan(fov/2);

    double x = dx;
    double y = -dy;
    double z = a;

    double len = sqrt(x*x+y*y+z*z);

    *sx = x/len;
    *sy = y/len;
    *sz = z/len;
}

void stereographicMap(int width, int height, double fov, double dx, double dy, double *sx, double *sy, double *sz)
{
    double diam = (double)height/2 / tan(fov/4);
    double rad = diam/2;

    double t = 2*rad*diam / (dx*dx + dy*dy + diam*diam);

    *sx = dx * t / rad;
    *sy = -dy * t / rad;
    *sz = (-rad + diam * t) / rad;
}


void lookuptable(B **buf, int width, int height, B *scrp, double fov, int map) {
  int x, y;
  for(y = 0;y<height;y++) for(x = 0;x<width;x++) {
    double dx = x-width/2;
    double dy = -(y-height/2);

    // map the current window coordinate to a ray vector
    double sx, sy, sz;
    if (map == 0) {
       perspectiveMap(width, height, fov, dx, dy, &sx, &sy, &sz);
    }
    else if (map == 1) {
       fisheyeMap(width, height, fov, dx, dy, &sx, &sy, &sz);
    }
    else if (map == 2) {
       cylinderMap(width, height, fov, dx, dy, &sx, &sy, &sz);
    }
    else {
       stereographicMap(width, height, fov, dx, dy, &sx, &sy, &sz);
    }

    // determine which side of the box we need
    double abs_x = fabs(sx);
    double abs_y = fabs(sy);
    double abs_z = fabs(sz);			
    int side;
    double xs=0, ys=0;
    if (abs_x > abs_y) {
      if (abs_x > abs_z) { side = ((sx > 0.0) ? BOX_RIGHT : BOX_LEFT);   }
      else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
    } else {
      if (abs_y > abs_z) { side = ((sy > 0.0) ? BOX_TOP   : BOX_BOTTOM); }
      else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
    }

    #define RC(x) ((x / 2.06) + 0.5)
    #define R2(x) ((x / 2.03) + 0.5)

    // scale up our vector [x,y,z] to the box
    switch(side) {
      case BOX_FRONT:  xs = RC( sx /  sz); ys = R2( sy /  sz); break;
      case BOX_BEHIND: xs = RC(-sx / -sz); ys = R2( sy / -sz); break;
      case BOX_LEFT:   xs = RC( sz / -sx); ys = R2( sy / -sx); break;
      case BOX_RIGHT:  xs = RC(-sz /  sx); ys = R2( sy /  sx); break;
      case BOX_TOP:    xs = RC( sx /  sy); ys = R2( sz / -sy); break; //bot
      case BOX_BOTTOM: xs = RC(-sx /  sy); ys = R2( sz / -sy); break; //top??
    }

    if (xs <  0.0) xs = 0.0;
    if (xs >= 1.0) xs = 0.999;
    if (ys <  0.0) ys = 0.0;
    if (ys >= 1.0) ys = 0.999;
    *buf++=scrp+(((int)(xs*(double)width))+
                 ((int)(ys*(double)height))*width)+
                 side*width*height;
  };
};

void renderside(B* bufs, double yaw, double pitch, double roll, int side) {
  struct my_angles a;
  get_ypr(RAD(yaw), RAD(pitch), RAD(roll), side, &a);
  if (side == BOX_RIGHT) { a.roll = -a.roll; a.pitch = -a.pitch; }
  if (side == BOX_LEFT)  { a.roll = -a.roll; a.pitch = -a.pitch; }
  if (side == BOX_TOP)   { a.yaw += 180.0; a.pitch = 180.0 - a.pitch; }
  r_refdef.viewangles[YAW] = a.yaw;
  r_refdef.viewangles[PITCH] = a.pitch;
  r_refdef.viewangles[ROLL] = a.roll;
  rendercopy((int *)bufs);
};

//extern int istimedemo;

void R_RenderView_fisheye() {
  int width = vid.width; //r_refdef.vrect.width;
  int height = vid.height; //r_refdef.vrect.height;
  int scrsize = width*height;
  int fov = (int)ffov.value;
  int views = (int)fviews.value;
  int map = (int)fmap.value;
  double yaw = r_refdef.viewangles[YAW];
  double pitch = r_refdef.viewangles[PITCH];
  double roll = 0;//r_refdef.viewangles[ROLL];
  static int pwidth = -1;
  static int pheight = -1;
  static int pfov = -1;
  static int pviews = -1;
  static int pmap = -1;
  static B *scrbufs = NULL;  
  static B **offs = NULL;
  static int demonum = 0;
  char framename[100];
  //Con_Printf("renderfisheye: %d %d %d\n",vid.height,vid.width,vid.rowbytes);

  if(fov<1) fov = 1;

  if(pwidth!=width || pheight!=height || pfov!=fov || pmap!=map) {
    if(scrbufs) free(scrbufs);
    if(offs) free(offs);
    scrbufs = (B*)malloc(scrsize*6); // front|right|back|left|top|bottom
    offs = (B**)malloc(scrsize*sizeof(B*));
    if(!scrbufs || !offs) exit(1); // the rude way
    pwidth = width;
    pheight = height;
    pfov = fov;
    pmap = map;
    lookuptable(offs,width,height,scrbufs,((double)fov)*PI/180.0, map);
  };

  if(views!=pviews) {
    int i;
    pviews = views;
    for(i = 0;i<scrsize*6;i++) scrbufs[i] = 0;
  };

  switch(views) {
    case 6:  renderside(scrbufs+scrsize*2,yaw,pitch,roll, BOX_BEHIND);
    case 5:  renderside(scrbufs+scrsize*5,yaw,pitch,roll, BOX_BOTTOM);
    case 4:  renderside(scrbufs+scrsize*4,yaw,pitch,roll, BOX_TOP);
    case 3:  renderside(scrbufs+scrsize*3,yaw,pitch,roll, BOX_LEFT);
    case 2:  renderside(scrbufs+scrsize,  yaw,pitch,roll, BOX_RIGHT);
    default: renderside(scrbufs,          yaw,pitch,roll, BOX_FRONT);
  };

  r_refdef.viewangles[YAW] = yaw;
  r_refdef.viewangles[PITCH] = pitch;
  r_refdef.viewangles[ROLL] = roll;
  renderlookup(offs,scrbufs);

  /*
  if(istimedemo) { 
    sprintf(framename,"anim/ani%05d.pcx",demonum++);
    //Con_Printf("attempting to write %s\n",framename);
    WritePCXfile(framename,vid.buffer,vid.width,vid.height,vid.rowbytes,host_basepal);
  } else {
    demonum = 0;
  }
  */
};


// FISHEYE END EDIT
