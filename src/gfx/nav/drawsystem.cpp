#include <set>
#include <vector>
#include <algorithm>
#include "vsfilesystem.h"
#include "vs_globals.h"
#include "vegastrike.h"
#include "gfx/gauge.h"
#include "gfx/cockpit.h"
#include "universe.h"
#include "star_system.h"
#include "cmd/unit_generic.h"
#include "cmd/unit_factory.h"
#include "cmd/collection.h"
#include "gfx/hud.h"
#include "gfx/vdu.h"
#include "lin_time.h" //for fps
#include "config_xml.h"
#include "lin_time.h"
#include "cmd/images.h"
#include "cmd/script/mission.h"
#include "cmd/script/msgcenter.h"
#include "cmd/ai/flyjoystick.h"
#include "cmd/ai/firekeyboard.h"
#include "cmd/ai/aggressive.h"
#include "main_loop.h"
#include <assert.h>     //needed for assert() calls
#include "savegame.h"
#include "gfx/animation.h"
#include "gfx/mesh.h"
#include "universe_util.h"
#include "in_mouse.h"
#include "gui/glut_support.h"
#include "networking/netclient.h"
#include "cmd/unit_util.h"
#include "math.h"
#include "save_util.h"
#include "navscreen.h"
#include "gfx/masks.h"
#include "galaxy_gen.h"

//**********************************
//Main function for drawing a CURRENT system
//works :
//scans all items, records min + max coords of the system, for relevant items
//rescans, and enlists the found items that it wants drawn
//-	items with mouse over them will go into a mouselist.
//draws the draw lists, with the mouse lists cycled 'n' times (according to kliks)
//**********************************

void NavigationSystem::DrawSystem()
{
    UniverseUtil::PythonUnitIter bleh = UniverseUtil::getUnitList();
    if ( !(*bleh) )
        return;
//string mystr ("3d "+XMLSupport::tostring (system_view));
//UniverseUtil::IOmessage (0,"game","all",mystr);

    //what's my name
    //***************************
    TextPlane systemname;       //will be used to display shits names
    int faction = FactionUtil::GetFactionIndex( UniverseUtil::GetGalaxyFaction( _Universe->activeStarSystem()->getFileName() ) );
    //GFXColor factioncolor = factioncolours[faction];
    string    systemnamestring = "#ff0000Sector: #ffff00"+getStarSystemSector( _Universe->activeStarSystem()->getFileName() )
                                 +"  #ff0000Current System: #ffff00"+_Universe->activeStarSystem()->getName()+" ("
                                 +FactionUtil::GetFactionName( faction )
                                 +"#ffff00)";
    int    length   = systemnamestring.size();
    float  offset   = (float(length)*0.0035f);      // approx half the title width, for centering
    float  midx     = (screenskipby4[0]+screenskipby4[1])/2.0f;
    systemname.SetPos( midx-offset, 0.96f );     //centred, inset below the top edge
    systemname.col = GFXColor( 1, 1, .7, 1 );
    systemname.SetText( systemnamestring );
//systemname.SetCharSize(1, 1);
    static float background_alpha =
        XMLSupport::parse_float( vs_config->getVariable( "graphics", "hud", "text_background_alpha", "0.0625" ) );
    GFXColor     tpbg = systemname.bgcol;
    bool automatte    = (0 == tpbg.a);
    if (automatte) systemname.bgcol = GFXColor( 0, 0, 0, background_alpha );
    systemname.Draw( systemnamestring, 0, true, false, automatte );
    systemname.bgcol = tpbg;
    //***************************

//navdrawlist mainlist(0, screenoccupation, factioncolours);		//	lists of items to draw
//mainlist.unselectedalpha = unselectedalpha;
    navdrawlist mouselist( 1, screenoccupation, factioncolours );       //lists of items to draw that are in mouse range

    QVector     pos;    //item position

    // Coherent camera model: zoom/pan/rotate through the system NavMap.
    Adjust3dTransformation( 1 );
    //Set up first item to compare to + centres
    //**********************************
    while ( (*bleh) && ( _Universe->AccessCockpit()->GetParent() != (*bleh) )
           && ( UnitUtil::isSun( *bleh ) || !UnitUtil::isSignificant( *bleh ) ) )                                                                       //no sun's in initial setup
        ++bleh;
    if ( !(*bleh) )      //nothing there that's significant, just do it all
        bleh = UniverseUtil::getUnitList();
    // Centre the map's content in the FREE area (left screen edge to the button
    // column), not the geometric screen centre (which sits under the buttons).
    float center_nav_x = -0.25f;   // midpoint of [-1, 0.5]; the buttons start at 0.5
    float center_nav_y = ( (screenskipby4[2]+screenskipby4[3])/2 );
    //**********************************
    // Auto-frame the system camera to the current system's significant units
    // the first time (and whenever the current system changes).
    //
    // Outlier-robust: the old code centred on the min/max box midpoint, which a
    // single far unit (e.g. a jump gate or a distant moon) could yank far from
    // the actual cluster, leaving the interesting units tiny or off-screen. We
    // centre on the player's ship (the natural focus) and frame to the cluster
    // of significant units near it, rejecting far outliers.
    if (systemNeedsRefit) {
        // Collect significant (non-sun) unit positions; keep the player.
        Unit *player = _Universe->AccessCockpit()->GetParent();
        std::vector< QVector > pts;
        for (un_iter u = UniverseUtil::getUnitList(); (*u); ++u) {
            if ( UnitUtil::isSun( *u ) )
                continue;
            if ( UnitUtil::isSignificant( *u ) || (player == (*u)) )
                pts.push_back( (*u)->Position() );
        }
        if (pts.empty())
            pts.push_back( Vector( 0, 0, 0 ) );

        // Center on the player when present, else the mean of the cluster.
        QVector center = player ? player->Position() : QVector( 0, 0, 0 );
        if (!player) {
            for (size_t i = 0; i < pts.size(); ++i)
                center += pts[i];
            center = center / double(pts.size());
        }

        // Extent = spread of ALL significant units, so nothing meaningful (bases,
        // planets, stations, jump points) is left off the framed view. The old
        // 3x-median rejection could drop a legitimately far base/planet, leaving
        // it off-screen ("flashes on the edge"). For a free-fly camera the
        // auto-frame only matters on first open, so frame to include everything
        // significant.
        float maxdist = 0.0f;
        for (size_t i = 0; i < pts.size(); ++i) {
            float d = (pts[i]-center).Magnitude();
            if (d > maxdist)
                maxdist = d;
        }
        if (maxdist <= 0.0f)
            maxdist = 1.0f;

        // Fit the camera so maxdist fits the viewport, looking at the centre.
        systemCam.setFraming( center, maxdist, maxdist, maxdist, NAV_FIT_FOV );
        systemNeedsRefit = false;
    }
    DrawOriginOrientationTri( center_nav_x, center_nav_y, 1 );

/*
 *       string mystr ("max x "+XMLSupport::tostring (max_x));
 *       UniverseUtil::IOmessage (0,"game","all",mystr);
 *
 *       string mystr2 ("min x "+XMLSupport::tostring (min_x));
 *       UniverseUtil::IOmessage (0,"game","all",mystr2);
 *
 *       string mystr3 ("max y "+XMLSupport::tostring (max_y));
 *       UniverseUtil::IOmessage (0,"game","all",mystr3);
 *
 *       string mystr4 ("min y "+XMLSupport::tostring (min_y));
 *       UniverseUtil::IOmessage (0,"game","all",mystr4);
 *
 *       UniverseUtil::IOmessage (0,"game","all",mystrcy);
 */

    Unit *ThePlayer = ( UniverseUtil::getPlayerX( UniverseUtil::getCurrentPlayer() ) );

    // Buffer for drawn items, so overlapping clusters can be collapsed to the
    // largest object before drawing (instead of an expanding label list).
    struct NavItem { int type; float size, x, y; Unit *unit; double realSize; };
    std::vector< NavItem > drawn;
    drawn.reserve( 64 );

    //Enlist the items and attributes
    //**********************************
    navNearDist = 1e30;   // reset nearest-in-view distance for this frame
    un_iter blah = UniverseUtil::getUnitList();
    while (*blah) {
        //this draws the points

        //Retrieve unit data
        //**********************************
        string temp = (*blah)->name;

        pos = (*blah)->Position();

        float the_x, the_y, sscale, system_item_scale_temp;
        if ( !systemCam.project( pos, the_x, the_y, sscale ) ) {
            ++blah;
            continue;
        }
        the_x = center_nav_x + the_x;
        the_y = center_nav_y + the_y;
        {
            // DEBUG: which units reach the draw and their significance
            bool sig = UnitUtil::isSignificant( *blah );
            fprintf( stderr, "[NAVDRAW] '%s' isUnit=%d sig=%d fg=%s onscreen=%d\n",
                     std::string( (*blah)->name.get() ).c_str(), (int)(*blah)->isUnit(), sig,
                     UnitUtil::getFlightgroupNameCR( *blah ).c_str(),
                     TestIfInRange( screenskipby4[0], screenskipby4[1], screenskipby4[2], screenskipby4[3], the_x, the_y ) );
        }
        system_item_scale_temp = sscale;
        if (system_item_scale_temp > maximumitemscaleup)
            system_item_scale_temp = maximumitemscaleup;
        if (system_item_scale_temp < minimumitemscaledown)
            system_item_scale_temp = minimumitemscaledown;
        //IGNORE OFF SCREEN
        //**********************************
        if ( !TestIfInRange( screenskipby4[0], screenskipby4[1], screenskipby4[2], screenskipby4[3], the_x, the_y ) ) {
            ++blah;
            continue;
        }
        //**********************************

        //Now starts the test that determines the type of things and inserts
        //|
        //|
        //\/

        float insert_size = 0.0;
        int   insert_type = navambiguous;
        if ( (*blah)->isUnit() == UNITPTR ) {
            //unit
            /*if(UnitUtil::isPlayerStarship(*blah) > -1)	//	is a PLAYER SHIP
             *  {
             *       if (UnitUtil::isPlayerStarship (*blah)==UniverseUtil::getCurrentPlayer()) //	is THE PLAYER
             *       {
             *               insert_type = navcurrentplayer;
             *               insert_size = navcurrentplayersize;
             *       }
             *       else	//	is A PLAYER
             *       {
             *               insert_type = navplayer;
             *               insert_size = navplayersize;
             *       }
             *  }
             *  else	//	is a non player ship
             *  {*/
            if ( UnitUtil::isSignificant( *blah ) ) {
                //capship or station
                if ( (*blah)->GetComputerData().max_speed() == 0 ) {
                    //is this item STATIONARY?
                    insert_type = navstation;
                    insert_size = navstationsize;
                } else {
                    //it moves = capship
                    if ( ThePlayer->InRange( (*blah), false, false ) ) {
                        //only insert if in range
                        insert_type = navcapship;
                        insert_size = navcapshipsize;
                    } else {
                        //skip unit completely if not in range
                        ++blah;
                        continue;
                    }
                }
            } else {
                //fighter
                /*if(ThePlayer->InRange((*blah),false,false))	//	only insert if in range
                 *  {
                 *       insert_type = navfighter;
                 *       insert_size = navfightersize;
                 *  }
                 *  else	// skip unit completely if not in range
                 *  {
                 * ++blah;
                 *       continue;
                 *  }*/
                if (UnitUtil::isPlayerStarship( *blah ) > -1) {
                    //is THE PLAYER
                    insert_type = navfighter;
                    insert_size = navfightersize;
                } else {
                    //skip unit completely if not in range
                    ++blah;
                    continue;
                }
            }
            //}
        } else if ( (*blah)->isUnit() == PLANETPTR ) {
            //is it a PLANET?
            if ( UnitUtil::isSun( *blah ) ) {
                //is this a SUN?
                insert_type = navsun;
                insert_size = navsunsize;
            } else if ( !( (*blah)->GetDestinations().empty() ) ) {
                //is a jump point (has destinations)
                insert_type = navjump;
                insert_size = navjumpsize;
            } else {
                //its a planet
                insert_type = navplanet;
                insert_size = navplanetsize;
            }
        } else if ( (*blah)->isUnit() == MISSILEPTR ) {
            //a missile
            insert_type = navmissile;
            insert_size = navmissilesize;
        } else if ( (*blah)->isUnit() == ASTEROIDPTR ) {
            //an asteroid
            insert_type = navasteroid;
            insert_size = navasteroidsize;
        } else if ( (*blah)->isUnit() == NEBULAPTR ) {
            //a nebula
            insert_type = navnebula;
            insert_size = navnebulasize;
        } else {
            //undefined non unit
            insert_type = navambiguous;
            insert_size = navambiguoussize;
        }
        if ( system_item_scale_temp > (system_item_scale*3) )
            system_item_scale_temp = (system_item_scale*3);
        insert_size *= system_item_scale_temp;
        // Keep items above a minimum on-screen size so they stay visible when
        // zoomed out to a huge extent (e.g. two clusters far apart in a system).
        if (insert_size < NavMinItemSize())
            insert_size = NavMinItemSize();
        Unit *myunit = (*blah);

        // Track the distance to the nearest significant object IN VIEW (ahead of
        // the camera, roughly within the view cone) — used to scale zoom/pan so
        // the camera moves fast through empty space and finely near objects. Only
        // objects ahead of the camera count, so turning away from a planet doesn't
        // slow your zoom toward the next one.
        {
            bool sig = (insert_type == navplanet || insert_type == navstation
                        || insert_type == navjump || insert_type == navsun
                        || insert_type == navcapship);
            if (sig) {
                QVector toObj = pos - systemCam.position();
                if (toObj.Dot( systemCam.forward() ) > 0.0)
                    {
                        double d = toObj.Magnitude();
                        if (d < navNearDist)
                            navNearDist = d;
                    }
            }
        }

        // Buffer this drawable item. Overlapping clusters are collapsed to the
        // largest object after the loop (the player is always drawn), instead of
        // spreading into an expanding label list.
        NavItem item;
        item.type = insert_type;
        item.size = insert_size;
        item.x    = the_x;
        item.y    = the_y;
        item.unit = myunit;
        item.realSize = myunit ? myunit->rSize() : 0.0;   // actual sim size (for collapse ranking)
        drawn.push_back( item );

        ++blah;
    }

    // Collapse overlapping items: when several objects project to nearly the
    // same screen position, keep only the largest. The player (and the unit
    // currently under the mouse, for click-select) are always kept.
    //
    // The region radius is larger than the icon overlap so objects clustered in
    // a region collapse to a single marker instead of each drawing a label that
    // stacks into a long list. The "largest" is ranked by the object's REAL
    // simulation size (rSize), not its screen size — screen size is clamped to
    // the NavMinItemSize floor when zoomed out, so every icon looked equal and
    // nothing collapsed.
    const float CLUSTER_RAD = 0.05f;   // HUD units (~2.5% of half-screen)
    auto isKeeper = [&]( const NavItem &it ) {
        if (it.unit && UnitUtil::isPlayerStarship( it.unit ) > -1)
            return true;
        float tx = it.x, ty = it.y;
        return TestIfInRangeRad( tx, ty, it.size, mouse_x_current, mouse_y_current );
    };
    for (size_t i = 0; i < drawn.size(); ++i) {
        if (drawn[i].size < 0)            // already collapsed away
            continue;
        for (size_t j = i+1; j < drawn.size(); ++j) {
            if (drawn[j].size < 0)
                continue;
            float dx = drawn[i].x - drawn[j].x;
            float dy = drawn[i].y - drawn[j].y;
            float rr = CLUSTER_RAD;
            if (dx*dx + dy*dy < rr*rr) {
                bool ki = isKeeper( drawn[i] );
                bool kj = isKeeper( drawn[j] );
                if (ki && kj)
                    continue;                       // keep both (player + hovered)
                // Keep the object with the LARGER REAL simulation size (rSize),
                // so a planet wins over an asteroid/fighter. On a tie drop j
                // (unless j is a keeper). Never drop a keeper.
                if (drawn[j].realSize > drawn[i].realSize) {
                    if (!ki) drawn[i].size = -1;    // j bigger, drop i (unless i is a keeper)
                } else if (drawn[j].realSize == drawn[i].realSize) {
                    if (!kj) drawn[j].size = -1;    // tie, drop j (unless j is a keeper)
                } else {
                    if (!kj) drawn[j].size = -1;    // i bigger, drop j
                }
            }
        }
    }
    // Draw the survivors.
    for (size_t i = 0; i < drawn.size(); ++i) {
        if (drawn[i].size < 0)
            continue;
        NavItem &it = drawn[i];
        if ( _Universe->AccessCockpit()->GetParent()->Target() == it.unit ) {
            static float col[4] = {1, 0.3, 0.3, 0.8};
            static bool  init   = false;
            if (!init) {
                vs_config->getColor( "nav", "targeted_unit", col, true );
                init = true;
            }
            DrawTargetCorners( it.x, it.y, it.size, GFXColor( col[0], col[1], col[2], col[3] ) );
        }
        bool tests_in_range = TestIfInRangeRad( it.x, it.y, it.size, mouse_x_current, mouse_y_current );
        if (tests_in_range) {
            mouselist.insert( it.type, it.size, it.x, it.y, it.unit );
        } else {
            drawlistitem( it.type,
                          it.size,
                          it.x,
                          it.y,
                          it.unit,
                          screenoccupation,
                          false,
                          false,
                          unselectedalpha,
                          factioncolours );
        }
    }
    drawn.clear();
    //**********************************	//	done enlisting items and attributes
    //Adjust mouse list for 'n' kliks
    //**********************************
    //STANDARD	: (1 3 2) ~ [0] [2] [1]
    //VS			: (1 2 3) ~ [0] [1] [2]	<-- use this
    if (mouselist.get_n_contents() > 0) {
        //mouse is over a target when this is > 0
        if (mouse_wentdown[2] == 1)             //mouse button went down for mouse button 2(standard)
            rotations += 1;
    }
    if ( rotations >= mouselist.get_n_contents() )      //dont rotate more than there is
        rotations = 0;
    int r = 0;
    while (r < rotations) {
        //rotate whatver rotations, leaving n rotated items, tail on top
        mouselist.rotate();
        r += 1;
    }
    //**********************************
    //Draw the damn shit
    //**********************************
//mainlist.draw();	//	draw the items
//mainlist.wipe();	//	whipe the list
    //**********************************
    //Check for selection query
    //give back the selected tail IF there is one
    //IF given back, undo the selection state
    //**********************************
    if ( 1 || checkbit( buttonstates, 1 ) ) {
        //button #2 is down, wanting a (selection)
        if (mouselist.get_n_contents() > 0) {
            //mouse is over a target when this is > 0
            if (mouse_wentdown[0] == 1) {
                //mouse button went down for mouse button 1
                currentselection = mouselist.gettailunit();
                unsetbit( buttonstates, 1 );
                //JUST FOR NOW, target == current selection. later it'll be used for other shit, that will then set target.
                if ( currentselection.GetUnit() ) {
                    ( UniverseUtil::getPlayerX( UniverseUtil::getCurrentPlayer() ) )->Target( currentselection.GetUnit() );
                    ( UniverseUtil::getPlayerX( UniverseUtil::getCurrentPlayer() ) )->LockTarget( currentselection.GetUnit() );
                }
            }
        }
    }
    //**********************************

    //Clear the lists
    //**********************************
    mouselist.draw();           //draw mouse over'd items
    mouselist.wipe();           //whipe mouse over'd list
    //**********************************
}
//**********************************

