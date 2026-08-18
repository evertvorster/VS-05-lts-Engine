#include <set>
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
#include "gfx/vdu.h"
#include "navscreen.h"
#include "gfx/masks.h"
#include "navitemstodraw.h"
#include "navcomputer.h"
#include "navpath.h"

//This sets up the items in the navscreen
//**********************************

NavigationSystem::NavigationSystem()
{
    draw = -1;
    whattodraw     = (1|2);
    pathman        = new PathManager();
    navcomp        = new NavComputer( this );
    for (int i = 0; i < NAVTOTALMESHCOUNT; i++)
        mesh[i] = NULL;
    factioncolours = NULL;
}

NavigationSystem::~NavigationSystem()
{
    draw = 0;
    //delete mesh;
    delete screenoccupation;
    delete mesh[0];
    delete mesh[1];
    delete mesh[2];
    delete mesh[3];
    delete mesh[4];
    delete mesh[5];
    delete mesh[6];
    delete mesh[7];
    delete factioncolours;
}
void NavigationSystem::mouseDrag( int x, int y )
{
    mousex = x;
    mousey = y;
}
void NavigationSystem::mouseMotion( int x, int y )
{
    mousex = x;
    mousey = y;
}
void NavigationSystem::mouseClick( int button, int state, int x, int y )
{
    mousex = x;
    mousey = y;
    if (state == WS_MOUSE_DOWN)
        mousestat |= ( 1<<lookupMouseButton( button ) );
    else if (button != WS_WHEEL_UP && button != WS_WHEEL_DOWN)
        mousestat &= ( ~( 1<<lookupMouseButton( button ) ) );
}

void NavigationSystem::Setup()
{
    _Universe->AccessCockpit()->visitSystem( _Universe->activeStarSystem()->getFileName() );

    rotations  = 0;

    minimumitemscaledown = 0.2;
    maximumitemscaleup   = 3.0;

    // Coherent camera model: one free-fly NavMap camera per view. Start in 3D
    // orbit (the only view).
    galaxyNeedsRefit = true;         // re-fit to extent on first draw
    systemNeedsRefit = true;

    scrolloffset = 0;

    path_view    = PATH_ON;
    system_item_scale = 1.0;
    mouse_previous_state[0] = 0;        //could have used a loop, but this way the system uses immediate instead of R type.
    mouse_previous_state[1] = 0;
    mouse_previous_state[2] = 0;
    mouse_previous_state[3] = 0;
    mouse_previous_state[4] = 0;
    mouse_wentup[0]   = 0;
    mouse_wentup[1]   = 0;
    mouse_wentup[2]   = 0;
    mouse_wentup[3]   = 0;
    mouse_wentup[4]   = 0;
    mouse_wentdown[0] = 0;
    mouse_wentdown[1] = 0;
    mouse_wentdown[2] = 0;
    mouse_wentdown[3] = 0;
    mouse_wentdown[4] = 0;
    mouse_x_previous  = ( -1+float(mousex)/(.5*g_game.x_resolution) );
    mouse_y_previous  = ( 1+float(-1*mousey)/(.5*g_game.y_resolution) );

    static int max_map_nodes = XMLSupport::parse_int( vs_config->getVariable( "graphics", "max_map_nodes", "256000" ) );
    systemIter.init( UniverseUtil::getSystemFile(), max_map_nodes );
    sectorIter.init( systemIter );
    // Sync the current-system index to the player's actual location, so the
    // galaxy view labels the right system (and the fit centres correctly).
    setCurrentSystem( _Universe->activeStarSystem()->getFileName() );
    systemselectionindex   = 0;
    sectorselectionindex   = 0;
    destinationsystemindex = 0;
    currentsystemindex     = 0;
    setFocusedSystemIndex( 0 );

    static int time_to_helpscreen = XMLSupport::parse_int( vs_config->getVariable( "general", "times_to_show_help_screen", "3" ) );
    buttonstates = 0;
    if (getSaveData( 0, "436457r1K3574r7uP71m35", 0 ) <= time_to_helpscreen)
        whattodraw = 0;
    else
        whattodraw = (1|2);
    currentselection = NULL;
    navNearDist      = 1.0;
    factioncolours   = new GFXColor[FactionUtil::GetNumFactions()];
    unselectedalpha  = 1.0;

    sectorOffset     = systemOffset = 0;

    unsigned int p;
    for (p = 0; p < FactionUtil::GetNumFactions(); p++) {
        factioncolours[p].r = 1;
        factioncolours[p].g = 1;
        factioncolours[p].b = 1;
        factioncolours[p].a = 1;
    }
    for (p = 0; p < NAVTOTALMESHCOUNT; p++)
        meshcoordinate_x[p] = 0.0;
    for (p = 0; p < NAVTOTALMESHCOUNT; p++)
        meshcoordinate_y[p] = 0.0;
    for (p = 0; p < NAVTOTALMESHCOUNT; p++)
        meshcoordinate_z[p] = 0.0;
    for (p = 0; p < NAVTOTALMESHCOUNT; p++)
        meshcoordinate_z_delta[p] = 0.0;
    //select target
    //NAV/MISSION toggle
    //

//HERE GOES THE PARSING
//*************************
// Map region (normalised screen bounds), set directly in C++ — the layout is no
// longer moddable via navdata.xml. FULL SCREEN; the button column overlays the
// right edge on top of the map.
    screenskipby4[0]   = 0;
    screenskipby4[1]   = 1;
    screenskipby4[2]   = 0;
    screenskipby4[3]   = 1;

    buttonskipby4_1[0] = .75;
    buttonskipby4_1[1] = .95;
    buttonskipby4_1[2] = .85;
    buttonskipby4_1[3] = .90;

    buttonskipby4_2[0] = .75;
    buttonskipby4_2[1] = .95;
    buttonskipby4_2[2] = .75;
    buttonskipby4_2[3] = .80;

    buttonskipby4_3[0] = .75;
    buttonskipby4_3[1] = .95;
    buttonskipby4_3[2] = .65;
    buttonskipby4_3[3] = .70;

    buttonskipby4_4[0] = .75;
    buttonskipby4_4[1] = .95;
    buttonskipby4_4[2] = .55;
    buttonskipby4_4[3] = .60;

    buttonskipby4_5[0] = .75;
    buttonskipby4_5[1] = .95;
    buttonskipby4_5[2] = .45;
    buttonskipby4_5[3] = .50;
    // No navdata.xml — the map region and button layout are fixed in C++ above.
    ScreenToCoord( screenskipby4[0] );
    ScreenToCoord( screenskipby4[1] );
    ScreenToCoord( screenskipby4[2] );
    ScreenToCoord( screenskipby4[3] );

    ScreenToCoord( buttonskipby4_1[0] );
    ScreenToCoord( buttonskipby4_1[1] );
    ScreenToCoord( buttonskipby4_1[2] );
    ScreenToCoord( buttonskipby4_1[3] );

    ScreenToCoord( buttonskipby4_2[0] );
    ScreenToCoord( buttonskipby4_2[1] );
    ScreenToCoord( buttonskipby4_2[2] );
    ScreenToCoord( buttonskipby4_2[3] );

    ScreenToCoord( buttonskipby4_3[0] );
    ScreenToCoord( buttonskipby4_3[1] );
    ScreenToCoord( buttonskipby4_3[2] );
    ScreenToCoord( buttonskipby4_3[3] );

    ScreenToCoord( buttonskipby4_4[0] );
    ScreenToCoord( buttonskipby4_4[1] );
    ScreenToCoord( buttonskipby4_4[2] );
    ScreenToCoord( buttonskipby4_4[3] );

    ScreenToCoord( buttonskipby4_5[0] );
    ScreenToCoord( buttonskipby4_5[1] );
    ScreenToCoord( buttonskipby4_5[2] );
    ScreenToCoord( buttonskipby4_5[3] );

//reverse = XMLSupport::parse_bool (vs_config->getVariable ("joystick","reverse_mouse_spr","true"))?1:-1;

    if ( (screenskipby4[1]-screenskipby4[0]) < (screenskipby4[3]-screenskipby4[2]) )
        system_item_scale *= (screenskipby4[1]-screenskipby4[0]);            //is actually over 1, which is itself
    else
        system_item_scale *= (screenskipby4[3]-screenskipby4[2]);
    screenoccupation = new navscreenoccupied( screenskipby4[0], screenskipby4[1], screenskipby4[2], screenskipby4[3], 1 );

    //Get special colors from the config
    float tempcol1[4] = {1, 0.3, 0.3, 1.0};
    vs_config->getColor( "nav", "current_system", tempcol1, true );
    currentcol     = GFXColor( tempcol1[0], tempcol1[1], tempcol1[2], tempcol1[3] );
    float tempcol2[4] = {1, 0.77, 0.3, 1.0};
    vs_config->getColor( "nav", "destination_system", tempcol2, true );
    destinationcol = GFXColor( tempcol2[0], tempcol2[1], tempcol2[2], tempcol2[3] );
    float tempcol3[4] = {0.3, 1, 0.3, 1.0};
    vs_config->getColor( "nav", "selection_system", tempcol3, true );
    selectcol = GFXColor( tempcol3[0], tempcol3[1], tempcol3[2], tempcol3[3] );
    float tempcol4[4] = {1, 0.3, 0.3, 1.0};
    vs_config->getColor( "nav", "path_system", tempcol4, true );
    pathcol   = GFXColor( tempcol4[0], tempcol4[1], tempcol4[2], tempcol4[3] );

    navcomp->init();
}

//**********************************

//This is the main draw loop for the nav screen
//**********************************
void NavigationSystem::Draw()
{
    if ( !CheckDraw() )
        return;
    if (_Universe->AccessCockpit()->GetParent() == NULL)
        return;
    
    // MODERN: the physical nav-panel mesh is removed — the nav screen is now a
    // clean 2D interface drawn over an opaque black backdrop (no camera/lightmap
    // setup needed; the content renders in HUD screen space).
    GFXBlendMode( SRCALPHA, INVSRCALPHA );
    GFXColor4f( 1, 1, 1, 1 );
    GFXDisable( TEXTURE0 );
    GFXDisable( TEXTURE1 );
    GFXDisable( LIGHTING );

    GFXHudMode( true );
    GFXDisable( DEPTHTEST );
    GFXDisable( DEPTHWRITE );
    //**********************************

    // Full-screen opaque black backdrop so the live game behind is fully hidden.
    // (Modern nav screen: no translucent shade, pure black.)
    {
        GFXColor4f( 0, 0, 0, 1 );
        static const float bgverts[4*3] = {
            -1, -1, 0,   1, -1, 0,   1, 1, 0,   -1, 1, 0
        };
        GFXDraw( GFXQUAD, bgverts, 4, 3, 0, 0 );
        GFXColor4f( 1, 1, 1, 1 );
    }

    screenoccupation->reset();

    //Save current mouse location
    //**********************************
    mouse_x_current = ( -1+float(mousex)/(.5*g_game.x_resolution) );
    mouse_y_current = ( 1+float(-1*mousey)/(.5*g_game.y_resolution) );
    //**********************************

    //Set Mouse
    //**********************************
    SetMouseFlipStatus();       //define bools 'mouse_wentdown[]' 'mouse_wentup[]'
    //**********************************
    //Draw the Navscreen Functions
    //**********************************
    if ( checkbit( whattodraw, 1 ) ) {
        if ( checkbit( whattodraw, 2 ) ) {
            DrawGalaxy();
        } else {
            DrawSystem();
        }
    } else {
        if ( checkbit( whattodraw, 3 ) )
            DrawSectorList();
        else if ( checkbit( whattodraw, 2 ) )
            DrawShip();
        else
            DrawMission();
    }
    //**********************************

    DrawObjectives();

    //Draw Button Outlines
    //**********************************
    bool outlinebuttons = 1;   // always draw button outlines (modern flat buttons)
    DrawButton( buttonskipby4_1[0], buttonskipby4_1[1], buttonskipby4_1[2], buttonskipby4_1[3], 1, outlinebuttons );
    DrawButton( buttonskipby4_2[0], buttonskipby4_2[1], buttonskipby4_2[2], buttonskipby4_2[3], 2, outlinebuttons );
    DrawButton( buttonskipby4_3[0], buttonskipby4_3[1], buttonskipby4_3[2], buttonskipby4_3[3], 3, outlinebuttons );
    DrawButton( buttonskipby4_4[0], buttonskipby4_4[1], buttonskipby4_4[2], buttonskipby4_4[3], 4, outlinebuttons );
    DrawButton( buttonskipby4_5[0], buttonskipby4_5[1], buttonskipby4_5[2], buttonskipby4_5[3], 5, outlinebuttons );
    //**********************************

    // Controls help — a small overlay at the bottom of the nav screen.
    // (No joystick support yet.)
    static bool draw_nav_help = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "draw_nav_help", "true" ) );
    if (draw_nav_help) {
        float help_y = -0.88f;
        float midx   = (screenskipby4[0]+screenskipby4[1])/2.0f;
        GFXColor helpcol( 0.7f, 0.7f, 0.7f, 0.85f );
        drawdescription( "Mouse:  right-drag = rotate   left/mid-drag = pan   wheel = zoom",
                         midx, help_y, 0.6f, 0.6f, true, screenoccupation, helpcol );
        drawdescription( "Keys:   arrows = pan   Shift+arrows = rotate   Alt+up/down = zoom   Alt+left/right = pan",
                         midx, help_y+0.06f, 0.6f, 0.6f, true, screenoccupation, helpcol );
    }

    //Draw the screen basics
    //**********************************
    DrawCursor( mouse_x_current, mouse_y_current, .1, .2, GFXColor( 1, 1, 1, 0.5 ) );
    //**********************************

    //Save current mouse location as previous for next cycle
    //**********************************
    mouse_x_previous = ( -1+float(mousex)/(.5*g_game.x_resolution) );
    mouse_y_previous = ( 1+float(-1*mousey)/(.5*g_game.y_resolution) );
    //**********************************

    GFXEnable( TEXTURE0 );
    GFXHudMode( false );
}
//**********************************

//This is the mission info screen
//**********************************
void NavigationSystem::DrawMission()
{
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    navdrawlist factionlist( 0, screenoccupation, factioncolours );

    float  deltax  = screenskipby4[1]-screenskipby4[0];
    float  deltay  = screenskipby4[3]-screenskipby4[2];
    float  originx = screenskipby4[0];    //left
    float  originy = screenskipby4[3];    //top
    vector< float > *killlist = &_Universe->AccessCockpit()->savegame->getMissionData( string( "kills" ) );
    string relationskills     = "Relations";
    if (killlist->size() > 0)
        relationskills += " | Kills";
    drawdescription( relationskills, ( originx+(0.1*deltax) ), (originy), 1, 1, 0, screenoccupation, GFXColor( .3, 1, .3, 1 ) );
    drawdescription( " ", ( originx+(0.1*deltax) ), (originy), 1, 1, 0, screenoccupation, GFXColor( .3, 1, .3, 1 ) );

    drawdescription( " ", ( originx+(0.3*deltax) ), (originy), 1, 1, 0, screenoccupation, GFXColor( .3, 1, .3, 1 ) );
    drawdescription( " ", ( originx+(0.3*deltax) ), (originy), 1, 1, 0, screenoccupation, GFXColor( .3, 1, .3, 1 ) );

    size_t numfactions    = FactionUtil::GetNumFactions();
    size_t i = 0;
    string factionname    = "factionname";
    float  relation       = 0.0;
    static string disallowedFactions = vs_config->getVariable( "graphics", "unprintable_factions", "" );
    static string disallowedExtension = vs_config->getVariable( "graphics", "unprintable_faction_extension", "citizen" );
    int    totkills       = 0;
    size_t fac_loc_before = 0, fac_loc = 0, fac_loc_after = 0;
    for (; i < numfactions; ++i) {
        factionname = FactionUtil::GetFactionName( i );
        if (factionname != "neutral" && factionname != "privateer" && factionname != "planets" && factionname != "upgrades") {
            if ( i < killlist->size() )
                totkills += (int) (*killlist)[i];
            if (factionname.find( disallowedExtension ) != string::npos)
                continue;
            fac_loc_after = 0;
            fac_loc = disallowedFactions.find( factionname, fac_loc_after );
            while (fac_loc != string::npos) {
                if (fac_loc > 0)
                    fac_loc_before = fac_loc-1;
                else
                    fac_loc_before = 0;
                fac_loc_after  = fac_loc+factionname.size();
                if ( (fac_loc == 0 || disallowedFactions[fac_loc_before] == ' ' || disallowedFactions[fac_loc_before]
                      == '\t')
                    && (disallowedFactions[fac_loc_after] == ' ' || disallowedFactions[fac_loc_after] == '\t'
                        || disallowedFactions[fac_loc_after] == '\0') )
                    break;
                fac_loc = disallowedFactions.find( factionname, fac_loc_after );
            }
            if (fac_loc != string::npos)
                continue;
            relation = UnitUtil::getRelationFromFaction( UniverseUtil::getPlayerX( UniverseUtil::getCurrentPlayer() ), i );

            //draw faction name
            const float *colors = FactionUtil::GetSparkColor( i );
            drawdescription( FactionUtil::GetFactionName(
                                i ), ( originx+(0.1*deltax) ), (originy), 1, 1, 0, screenoccupation,
                            GFXColor( colors[0], colors[1], colors[2], 1. ) );

            float  relation01 = relation*0.5+0.5;
            relation = ( (relation > 1 ? 1 : relation) < -1 ? -1 : relation );
            int    percent    = (int) (relation*100.0);
            string relationtext( XMLSupport::tostring( percent ) );
            if ( i < killlist->size() ) {
                relationtext += " | ";
                relationtext += XMLSupport::tostring( (int) (*killlist)[i] );
            }
            drawdescription( relationtext, ( originx+(0.3*deltax) ), (originy), 1, 1, 0, screenoccupation,
                            GFXColor( (1.0-relation01), (relation01), ( 1.0-( 2.0*Delta( relation01, 0.5 ) ) ), 1 ) );
        }
    }
    string relationtext( "Total Kills: " );
    relation      = 1;

    relationtext += XMLSupport::tostring( totkills );
    drawdescription( relationtext, ( originx+(0.2*deltax) ), ( originy-(0.95*deltay) ), 1, 1, 0, screenoccupation,
                    GFXColor( (1.0-relation), relation, ( 1.0-( 2.0*Delta( relation, 0.5 ) ) ), 1 ) );

//drawdescription(" Terran : ", (originx + (0.1*deltax)),(originy - (0.1*deltay)), 1, 1, 0, screenoccupation, GFXColor(.3,1,.3,1));
//drawdescription(" Rlaan : ", (originx + (0.1*deltax)),(originy - (0.1*deltay)), 1, 1, 0, screenoccupation, GFXColor(1,.3,.3,1));
//drawdescription(" Aera : ", (originx + (0.1*deltax)),(originy - (0.1*deltay)), 1, 1, 0, screenoccupation, GFXColor(.3,.3,1,1));

//float love_from_terran = FactionUtil::getRelation(1);
//float love_from_rlaan = FactionUtil::getRelation(2);
//float love_from_aera = FactionUtil::getRelation(3);

    TextPlane displayname;
    displayname.col = GFXColor( 1, 1, 1, 1 );
    displayname.SetSize( .42, -.7 );
    displayname.SetPos( originx+(.1*deltax)+.37, originy /*+(1*deltay)*/ );
    std::string text;
    if (active_missions.size() > 1) {
        for (unsigned int i = 1; i < active_missions.size(); ++i) {
            text += active_missions[i]->mission_name+":\n";
            for (unsigned int j = 0; j < active_missions[i]->objectives.size(); ++j)
                text += active_missions[i]->objectives[j].objective+": "
                        +XMLSupport::tostring( (int) (active_missions[i]->objectives[j].completeness*100) )+"%\n";
        }
        text += "\n";
    }
    text +=
        "#FFA000     PRESS SHIFT-M TO TOGGLE THIS MENU    \n\n\n\n#000000*******#00a6FFVega Strike 0.5#000000*********\nWelcome to VS. Your ship undocks stopped; #8080FFArrow keys/mouse/joystick#000000 steer your ship. Use #8080FF+#000000 & #8080FF-#000000 to adjust cruise control, or #8080FF/#000000 & #8080FF[backspace]#000000 to go to max governor setting or full-stop, respectively. Use #8080FFy#000000 to toggle between maneuver and travel settings for your relative velocity governors. Use #8080ff[home]#000000 & #8080FF[end]#000000 to set and unset velocity reference point to the current target (non-hostile targets only). Use #8080FFTab#000000 to activate Overdrive(if present).\n\nPress #8080FFn#000000 to cycle nav points, #8080FFt#000000 to cycle targets, and #8080FFp#000000 to target objects in front of you.\n\n#8080FF[space]#000000 fires guns, and #8080ff[Enter]#000000 fires missiles.\n\nThe #8080FFa#000000 key activates SPEC drive for insystem FTL.\nInterstellar Travel requires a #FFBB11 jump drive#000000 and #FFBB11FTL Capacitors#000000 to be installed. To jump, fly into the green wireframe nav-marker; hit #8080FFj#000000 to jump to the linked system.\n\nTo dock, target a base, planet or large vessel and hail with #8080FF0#000000 to request docking clearance. When you get close, a green box will appear. Fly to the box. When inside the box, #8080FFd#000000 will dock.\n\n#FF0000If Vega Strike halts or acts oddly,#000000\n#FFFF00immediately#000000 post stderr.txt & stdout.txt\nto http://vegastrike.sourceforge.net/forums\nbefore you restart Vega Strike.\n";
    displayname.SetText( text );
    displayname.SetCharSize( 1, 1 );
    displayname.Draw();
/*
 *       string exitinfo("To exit help press #8080FFshift-M#000000\n#8080FFShift-M#000000 will bring up this\nhelp menu any time.\nThe right buttons access the galaxy and system maps");
 *
 *       displayname.SetSize (.6,-.8);
 *       displayname.SetPos(originx-.02,   originy-1.2);
 *       displayname.SetText (exitinfo);
 *       displayname.SetCharSize (1,1);
 *       displayname.Draw();*/
    GFXEnable( TEXTURE0 );
}
//**********************************

//This is the mission info screen
//**********************************
extern string MakeUnitXMLPretty( string str, Unit *un );
void NavigationSystem::DrawShip()
{
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    navdrawlist factionlist( 0, screenoccupation, factioncolours );

    float     deltax   = screenskipby4[1]-screenskipby4[0];
    float     originx  = screenskipby4[0]; //left
    float     originy  = screenskipby4[3]; //top
    string    writethis;
    Unit     *par;
    if ( ( par = _Universe->AccessCockpit()->GetParent() ) )
        writethis = MakeUnitXMLPretty( par->WriteUnitString(), par );
    TextPlane displayname;
    displayname.col = GFXColor( .3, 1, .3, 1 );
    displayname.SetSize( .7, -.8 );
    displayname.SetPos( originx-(.1*deltax), originy /*+(1*deltay)*/ );
    displayname.SetText( writethis );
    displayname.SetCharSize( 1, 1 );
    static float background_alpha =
        XMLSupport::parse_float( vs_config->getVariable( "graphics", "hud", "text_background_alpha", "0.0625" ) );
    GFXColor     tpbg = displayname.bgcol;
    bool automatte    = (0 == tpbg.a);
    if (automatte) displayname.bgcol = GFXColor( 0, 0, 0, background_alpha );
    displayname.Draw( writethis, 0, true, false, automatte );
    displayname.bgcol = tpbg;

//factionlist.drawdescription(writethis, (originx + (0.1*deltax)),(originy - (0.1*deltay)), 1, 1, 1, GFXColor(1,1,1,1));

    GFXEnable( TEXTURE0 );
}

void NavigationSystem::DrawSectorList()
{
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    float    deltax  = screenskipby4[1]-screenskipby4[0];
    float    deltay  = screenskipby4[3]-screenskipby4[2];
    float    originx = screenskipby4[0];  //left
    float    originy = screenskipby4[3];  //top
    float    width   = (deltax/6);
    float    height  = (0.031*deltay);
    const unsigned numRows = 26;
    float    the_x, the_y, the_x1, the_y1, the_x2, the_y2;
    GFXColor color;
    unsigned count, index, row;

    //Draw Title of Column
    drawdescription( "Sectors", originx+(0.5*width), originy-(0.0*deltay), 1, 1, 1, screenoccupation,
                    GFXColor( .3, 1, .3, 1 ) );

    //Draw Scroll Pieces
    color  = GFXColor( 0.7, 0.3, 0.3, 1.0 );

    the_x  = width*(0.5)+originx;
    the_y  = originy-(0.05*deltay);
    the_x1 = the_x-width/2;
    the_y1 = the_y-height;
    the_x2 = the_x+width/2;
    the_y2 = the_y;
    if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
        if (mouse_wentdown[0] == 1)             //mouse button went down for mouse button 1
            if (sectorOffset > 0)
                --sectorOffset;
    }
    drawdescription( "Up", the_x, the_y, 1, 1, 1, screenoccupation, color );

    the_x  = width*(0.5)+originx;
    the_y  = originy-(0.05*deltay)-height*(29);
    the_x1 = the_x-width/2;
    the_y1 = the_y-height;
    the_x2 = the_x+width/2;
    the_y2 = the_y;
    if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
        if (mouse_wentdown[0] == 1)             //mouse button went down for mouse button 1
            if ( sectorOffset < (sectorIter.size()-numRows) )
                ++sectorOffset;
    }
    drawdescription( "Down", the_x, the_y, 1, 1, 1, screenoccupation, color );

    count = 0;
    for (sectorIter.seek(); !sectorIter.done(); ++sectorIter) {
        bool drawable = false;
        for (unsigned i = 0; i < sectorIter->GetSubsystemSize(); i++)
            if ( systemIter[sectorIter->GetSubsystemIndex( i )].isDrawable() ) {
                drawable = true;
                break;
            }
        if (!drawable)
            continue;
        if ( ( count < sectorOffset ) || ( count >= (numRows+sectorOffset) ) ) {
            ++count;
            continue;
        }
        row    = (count-sectorOffset)%numRows;
        the_x  = width*(0.5)+originx;
        the_y  = originy-(0.05*deltay)-height*(row+2);
        the_x1 = the_x-width/2;
        the_y1 = the_y-height;
        the_x2 = the_x+width/2;
        the_y2 = the_y;
        if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
            if (mouse_wentdown[0] == 1) {
                //mouse button went down for mouse button 1
                sectorselectionindex = sectorIter.getIndex();
                systemOffset = 0;
            }
        }
        if (sectorIter.getIndex() == sectorselectionindex)
            color = selectcol;
        else
            color = GFXColor( 0.7, 0.3, 0.3, 1.0 );
        drawdescription( sectorIter->GetName(), the_x, the_y, 1, 1, 1, screenoccupation, color );
        ++count;
    }
    drawdescription( "Systems", originx+(1.5)*width, originy-(0.0*deltay), 1, 1, 1, screenoccupation,
                    GFXColor( .3, 1, .3, 1 ) );

    //Draw Scroll Pieces
    color  = GFXColor( 0.7, 0.3, 0.3, 1.0 );

    the_x  = width*(1.5)+originx;
    the_y  = originy-(0.05*deltay);
    the_x1 = the_x-width/2;
    the_y1 = the_y-height;
    the_x2 = the_x+width/2;
    the_y2 = the_y;
    if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
        if (mouse_wentdown[0] == 1)             //mouse button went down for mouse button 1
            if (systemOffset > 0)
                --systemOffset;
    }
    drawdescription( "Up", the_x, the_y, 1, 1, 1, screenoccupation, color );

    the_x  = width*(1.5)+originx;
    the_y  = originy-(0.05*deltay)-height*(29);
    the_x1 = the_x-width/2;
    the_y1 = the_y-height;
    the_x2 = the_x+width/2;
    the_y2 = the_y;
    if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
        if (mouse_wentdown[0] == 1)             //mouse button went down for mouse button 1
            if ( systemOffset < (sectorIter[sectorselectionindex].GetSubsystemSize()-numRows) )
                ++systemOffset;
    }
    drawdescription( "Down", the_x, the_y, 1, 1, 1, screenoccupation, color );

    count = 0;
    sectorIter.seek( sectorselectionindex );
    for (unsigned i = 0; i < sectorIter->GetSubsystemSize(); ++i) {
        index = sectorIter->GetSubsystemIndex( i );
        if ( !systemIter[index].isDrawable() )
            continue;
        if ( ( count < systemOffset ) || ( count >= (numRows+systemOffset) ) ) {
            ++count;
            continue;
        }
        row    = (count-systemOffset)%numRows;
        the_x  = width*(1.5)+originx;
        the_y  = originy-(0.05*deltay)-height*(row+2);
        the_x1 = the_x-width/2;
        the_y1 = the_y-height;
        the_x2 = the_x+width/2;
        the_y2 = the_y;
        if ( TestIfInRange( the_x1, the_x2, the_y1, the_y2, mouse_x_current, mouse_y_current ) ) {
            if (mouse_wentdown[0] == 1) {
                //mouse button went down for mouse button 1
                unsigned oldselection = systemselectionindex;
                systemselectionindex = index;
                if (systemselectionindex == oldselection)
                    setFocusedSystemIndex( systemselectionindex );
            }
        }
        if (index == destinationsystemindex)
            color = destinationcol;
        else if (index == focusedsystemindex)
            color = currentcol;
        else if (index == systemselectionindex)
            color = selectcol;
        else
            color = GFXColor( 0.7, 0.3, 0.3, 1.0 );
        string csector, csystem;
        Beautify( systemIter[index].GetName(), csector, csystem );

        drawdescription( csystem, the_x, the_y, 1, 1, 1, screenoccupation, color );
        ++count;
    }
}

void NavigationSystem::DrawObjectives()
{
    if ( checkbit( whattodraw, 4 ) )
        //Draw the objectives screen!
        DrawObjectivesTextPlane( &screen_objectives, scrolloffset, _Universe->AccessCockpit()->GetParent() );
}

//this sets weather to draw the screen or not
//**********************************
void NavigationSystem::SetDraw( bool n )
{
    if (draw == -1) {
        Setup();
        draw = 0;
    }
    if ( n != (draw == 1) ) {
        ClearPriorities();
        scrolloffset = 0;
        draw = n ? 1 : 0;
    }
}
//**********************************

//this gets rid of states that could be damaging
//**********************************
void NavigationSystem::ClearPriorities()
{
    unsetbit( buttonstates, 1 );
    currentselection = NULL;
//rx = 1.0;		//	resetting rotations is up to hitting the 2d/3d button
//ry = 1.0;
//rz = 0.0;
//rx_s = 1.0;
//ry_s = 1.0;
//rz_s = 0.0;
}
//**********************************

//This will set a wentdown and wentup flag just for the event of mouse button going down or up
//this is an FF test. not a state test.
//**********************************
void NavigationSystem::SetMouseFlipStatus()
{
//getMouseButtonStatus()&1 = mouse button 1 standard = button 1 VS
//getMouseButtonStatus()&2 = mouse button 3 standard = button 2 VS
//getMouseButtonStatus()&4 = mouse button 2 standard = button 3 VS
//getMouseButtonStatus()&8 = mouse wheel up
//getMouseButtonStatus()&16 = mouse wheel down

    //use the VS scheme, (1 2 3 4 5) , instead of standard (1 3 2 4 5)
    //state 0 = up
    //state 1 = down

    bool status = 0;
    int  i;
    for (i = 0; i < 5; i++) {
        status = ( getMouseButtonStatus()&(1<<i) ) ? 1 : 0;
        if ( (status == 1) && (mouse_previous_state[i] == 0) ) {
            mouse_wentdown[i] = 1;
            mouse_wentup[i]   = 0;
        } else if ( (status == 0) && (mouse_previous_state[i] == 1) ) {
            mouse_wentup[i]   = 1;
            mouse_wentdown[i] = 0;
        } else {
            mouse_wentup[i]   = 0;
            mouse_wentdown[i] = 0;
            if (i == 3 || i == 4)
                mousestat &= ( ~(1<<i) );
        }
    }
    for (i = 0; i < 5; i++)
        mouse_previous_state[i] = ( getMouseButtonStatus()&(1<<i) );            //button 'i+1' state VS
}

//**********************************

//returns a modified vector rotated by x y z radians
//**********************************
void NavigationSystem::setCurrentSystem( string newSystem )
{
    for (unsigned i = 0; i < systemIter.size(); ++i)
        if (systemIter[i].GetName() == newSystem) {
            setCurrentSystemIndex( i );
            break;
        }
}

void NavigationSystem::setFocusedSystemIndex( unsigned newSystemIndex )
{
    focusedsystemindex = newSystemIndex;
    // Re-fit the galaxy camera to the newly focused system's extent on next
    // draw (centre the map on it and re-frame the distance).
    galaxyNeedsRefit = true;
}

void NavigationSystem::setCurrentSystemIndex( unsigned newSystemIndex )
{
    currentsystemindex = newSystemIndex;
    // Re-fit the system camera to the new current system's extent on next draw.
    systemNeedsRefit = true;
    static bool AlwaysUpdateNavMap =
        XMLSupport::parse_bool( vs_config->getVariable( "graphics", "update_nav_after_jump", "false" ) );                          //causes occasional crash--only may have tracked it down
    if (AlwaysUpdateNavMap)
        pathman->updatePaths( PathManager::CURRENT );
}

void NavigationSystem::setDestinationSystemIndex( unsigned newSystemIndex )
{
    destinationsystemindex = newSystemIndex;
    pathman->updatePaths( PathManager::TARGET );
}

std::string NavigationSystem::getCurrentSystem()
{
    if ( factioncolours == NULL || focusedsystemindex >= systemIter.size() )
        return _Universe->activeStarSystem()->getFileName();
    return systemIter[currentsystemindex].GetName();
}
std::string NavigationSystem::getSelectedSystem()
{
    if ( factioncolours == NULL || focusedsystemindex >= systemIter.size() )
        return _Universe->activeStarSystem()->getFileName();
    return systemIter[systemselectionindex].GetName();
}
std::string NavigationSystem::getDestinationSystem()
{
    if ( factioncolours == NULL || focusedsystemindex >= systemIter.size() )
        return _Universe->activeStarSystem()->getFileName();
    return systemIter[destinationsystemindex].GetName();
}
std::string NavigationSystem::getFocusedSystem()
{
    if ( factioncolours == NULL || focusedsystemindex >= systemIter.size() )
        return _Universe->activeStarSystem()->getFileName();
    return systemIter[focusedsystemindex].GetName();
}

//Passes a draw button command, with colour
//Tests for a mouse over, to set colour
//**********************************
//1 = nav/mission
//2 = select currentselection
//3 = up
//4 = down
//5 = toggle prespective rezoom
//6 = toggle 2d/3d mode
int NavigationSystem::mousey = 0;
int NavigationSystem::mousex = 0;
int NavigationSystem::mousestat;

void NavigationSystem::DrawButton( float &x1, float &x2, float &y1, float &y2, int button_number, bool outline )
{
    float  mx = mouse_x_current;
    float  my = mouse_y_current;
    bool   inrange = TestIfInRange( x1, x2, y1, y2, mx, my );

    string label;
    if (button_number == 1) {
        label = "Nav/Info";
    } else if (button_number == 3) {
        label = "Target Selected";
    } else if ( checkbit( whattodraw, 1 ) ) {
        if (button_number == 2)
            label = "Path On/Off/Only";
        else if (button_number == 4)
            label = "Up";
        else if (button_number == 5)
            label = "Down";
    } else {
        if (button_number == 2)
            label = "Sectors";
        else if (button_number == 4)
            label = "Ship";
        else if (button_number == 5)
            label = "Mission";
    }
    TextPlane   a_label;
    a_label.col = GFXColor( 1, 1, 1, 1 );
    int length = label.size();
    float       offset = (float(length)*0.0065);
    float       xl     = (x1+x2)/2.0;
    float       yl     = (y1+y2)/2.0;
    a_label.SetPos( (xl-offset)-(checkbit( buttonstates, button_number-1 ) ? 0.006 : 0), (yl+0.025) );
    a_label.SetText( label );

    // Modern flat button: draw a subtle dark fill behind the label so it reads
    // as a button even though the physical panel mesh is gone. Uses the button
    // rect x1..y2; the label sits slightly above centre.
    {
        GFXColorf( GFXColor( 0, 0, 0, 0.6f ) );
        GFXDisable( TEXTURE0 );
        GFXDisable( LIGHTING );
        GFXBlendMode( SRCALPHA, INVSRCALPHA );
        const float bv[4*3] = {
            x1, y1, 0,   x2, y1, 0,   x2, y2, 0,   x1, y2, 0
        };
        GFXDraw( GFXQUAD, bv, 4, 3, 0, 0 );
        GFXColorf( GFXColor( 1, 1, 1, 1 ) );
    }

    static bool nav_button_labels =
        XMLSupport::parse_bool( vs_config->getVariable( "graphics", "draw_nav_button_labels", "true" ) );
    if (nav_button_labels) {
        static float background_alpha =
            XMLSupport::parse_float( vs_config->getVariable( "graphics", "hud", "text_background_alpha", "0.0625" ) );
        GFXColor     tpbg = a_label.bgcol;
        bool automatte    = (0 == tpbg.a);
        if (automatte) a_label.bgcol = GFXColor( 0, 0, 0, background_alpha );
        a_label.Draw( label, 0, true, false, automatte );
        a_label.bgcol = tpbg;
    }
    //!!! DEPRESS !!!
    if ( (inrange == 1) && (mouse_wentdown[0] == 1) ) {
        currentselection = NULL;                //any new button depression means no depression on map, no selection made

        //******************************************************
        //**                 DEPRESS FUNCTION                 **	DEPRESS ALL
        //******************************************************

        dosetbit( buttonstates, (button_number-1) );            //all buttons go down

        //******************************************************
    }
    //!!! RELEASE !!!
    if ( (inrange == 1) && ( checkbit( buttonstates, (button_number-1) ) ) && (mouse_wentup[0]) ) {
        //******************************************************
        //**                 MISSION MODE	                  **	UNSET BITS WHEN ENTERING MISSION MODE
        //******************************************************
        if ( !checkbit( whattodraw, 1 ) )
            unsetbit( buttonstates, (button_number-1) );                //all are up in mission mode
        else
            unsetbit( buttonstates, (button_number-1) );                //all are up in navigation mode
        //******************************************************
        //******************************************************
        //**                 BUTTON 1 FUNCTION                **	NAV-INFO vs STATUS-INFO
        //******************************************************
        if (button_number == 1)          //releasing #1, toggle the draw (nav / mission)
            flipbit( whattodraw, 1 );
        //******************************************************
        //******************************************************
        //**                 BUTTON 2 FUNCTION                **	PATH options
        //******************************************************
        if (button_number == 2) {
            //releasing #2, toggle the path viewing settings(off/on/only)
            if ( ( checkbit( whattodraw, 1 ) ) && ( checkbit( whattodraw, 2 ) ) )
                path_view = (path_view+1)%PATH_MAXIMUM;
            else if ( !checkbit( whattodraw, 1 ) )
                dosetbit( whattodraw, 3 );
        }
        //******************************************************
        //******************************************************
        //**                 BUTTON 3 FUNCTION                **	TARGET SELECTED SYSTEM
        //******************************************************
        if (button_number == 3) {
            //hit --TARGET--
            if ( ( ( checkbit( whattodraw, 1 ) ) && ( checkbit( whattodraw, 2 ) ) )             //Nav-Galaxy Mode
                || ( ( !checkbit( whattodraw, 1 ) ) && ( checkbit( whattodraw, 3 ) ) ) )              //Mission-Sector Mode
                setDestinationSystemIndex( systemselectionindex );
        }
        //******************************************************
        //******************************************************
        //**                 BUTTON 4 FUNCTION                **	UP
        //******************************************************
        if (button_number == 4) {
            //hit --UP--
            if ( checkbit( whattodraw, 1 ) ) {
                //if in nav system NOT mission
                dosetbit( whattodraw, 2 );                      //draw galaxy
                setFocusedSystemIndex( currentsystemindex );
                systemselectionindex = currentsystemindex;
            } else {
                //if in mission mode
                unsetbit( whattodraw, 3 );
                dosetbit( whattodraw, 2 );                      //draw shipstats
            }
        }
        //******************************************************
        //******************************************************
        //**                 BUTTON 5 FUNCTION                **	DOWN
        //******************************************************
        if (button_number == 5) {
            //hit --DOWN--
            if ( checkbit( whattodraw, 1 ) ) {
                //if in nav system NOT mission

                unsetbit( whattodraw, 2 );                      //draw system
            } else {
                //if in mission mode
                unsetbit( whattodraw, 3 );
                unsetbit( whattodraw, 2 );                      //draw mission
            }
        }
        //******************************************************
        //******************************************************
        //**                 (BUTTON 6 & 7 REMOVED)          **	2D/3D & AXIS no longer exist
        //******************************************************
    }
    //!!! OUT OF BOUNDS !!!
    //******************************************************
    //**                 OUT OF RANGE	                  **	ALL DIE
    //******************************************************
    if (inrange == 0)
        unsetbit( buttonstates, (button_number-1) );
    //******************************************************
    //******************************************************
    //**             TRACE OUTLINES FOR EZ SETUP          **	ARTIST DEV UTIL
    //******************************************************
    if (outline == 1) {
        if (inrange == 1) {
            if ( checkbit( buttonstates, (button_number-1) ) )
                DrawButtonOutline( x1, x2, y1, y2, GFXColor( 1, 0, 0, 1 ) );

            else
                DrawButtonOutline( x1, x2, y1, y2, GFXColor( 1, 1, 0, 1 ) );
        } else {
            if ( checkbit( buttonstates, (button_number-1) ) )
                DrawButtonOutline( x1, x2, y1, y2, GFXColor( 1, 0, 0, 1 ) );
            else
                DrawButtonOutline( x1, x2, y1, y2, GFXColor( 1, 1, 1, 1 ) );
        }
    }
    //******************************************************
}
//**********************************

//Draws the actual button outline — a thin rounded-rect line loop.
//**********************************
void NavigationSystem::DrawButtonOutline( float &x1, float &x2, float &y1, float &y2, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    // Rounded rectangle outline as a connected line strip: each corner is an
    // arc, then a straight segment runs along the edge to the next corner arc.
    const float r = 0.5f * std::min( x2-x1, y2-y1 ) * 0.08f;
    const int   seg = 3;
    float verts[ 4 * 4 * seg * 3 ];
    int n = 0;

    // Four corners bottom-left, bottom-right, top-right, top-left.
    float cx[4] = { x1+r, x2-r, x2-r, x1+r };
    float cy[4] = { y1+r, y1+r, y2-r, y2-r };
    float a0[4] = { 180.f, 270.f, 0.f, 90.f };

    // Helper lambda-ish via a local function is awkward in C++03; just loop.
    for (int c = 0; c < 4; ++c) {
        // arc of this corner
        for (int s = 0; s <= seg; ++s) {
            float ang = ( a0[c] + 90.f * ( float(s) / seg ) ) * (float)M_PI / 180.f;
            verts[n++] = cx[c] + r * cosf( ang );
            verts[n++] = cy[c] + r * sinf( ang );
            verts[n++] = 0;
        }
        // straight segment along the edge to the start of the NEXT corner arc
        int   nc  = (c+1)%4;
        float ang = a0[nc] * (float)M_PI / 180.f;
        verts[n++] = cx[nc] + r * cosf( ang );
        verts[n++] = cy[nc] + r * sinf( ang );
        verts[n++] = 0;
    }
    // close the loop back to the first point
    verts[n++] = verts[0]; verts[n++] = verts[1]; verts[n++] = verts[2];
    GFXDraw( GFXLINESTRIP, verts, n/3 );

    GFXEnable( TEXTURE0 );
}
//**********************************

template < class T >
static inline bool intersect( T x0, T y0, T x1, T y1, T sx0, T sy0, T sx1, T sy1, T &ansx, T &ansy )
{
    bool fxy = false;
    if ( ( (x1 == x0) && (sx1 == sx0) ) || ( (x1 == x0) && (y1 == y0) ) || ( (sx1 == sx0) && (sy1 == sy0) ) ) {
        //If both lines are vertical, then act as if they don't intersect.
        //If either one is a point, then for all practical purposes they do not intersect.
        return false;
    }
    if ( (x1 == x0) && (sy1 == sy0) ) {
        //Line 1 vertical, line 2 horizontal.
        ansx = x1;
        ansy = sy1;
        return ( (sx0 <= x1
                  && x1 <= sx1) || (sx1 <= x1 && x1 <= sx0) ) && ( (y0 <= sy1 && sy1 <= y1) || (y1 <= sy1 && sy1 <= y0) );
    }
    if ( (sx1 == sx0) && (y1 == y0) ) {
        //line 1 horizontal, Line 2 vertical.
        ansx = sx1;
        ansy = y1;
        return ( (x0 <= sx1
                  && sx1 <= x1) || (x1 <= sx1 && sx1 <= x0) ) && ( (sy0 <= y1 && y1 <= sy1) || (sy1 <= y1 && y1 <= sy0) );
    }
    //If either line is vertical (both was handled above), then flip the coordinate plane to prevent division by zero.
    if ( (x1 == x0) || (sx1 == sx0) ) {
        T temp = x0;
        x0   = y0;
        y0   = temp;
        temp = x1;
        x1   = y1;
        y1   = temp;
        fxy  = true;
        temp = sx0;
        sx0  = sy0;
        sy0  = temp;
        temp = sx1;
        sx1  = sy1;
        sy1  = temp;
        fxy  = true;
    }
    //Now we can be sure that no vertical lines exist.
    //Proceed with the operation.
    T m  = (y1-y0)/(x1-x0);
    T sm = (sy1-sy0)/(sx1-sx0);
    if (m == sm)
        //Parallel Lines
        return false;
    ansx = (m*x1-sm*sx1-y1+sy1)/(m-sm);
    ansy = (y1-m*x1+m*ansx);
    if ( ( (x0 <= ansx
            && ansx <= x1)
          || (x1 <= ansx && ansx <= x0) ) && ( (sx0 <= ansx && ansx <= sx1) || (sx1 <= ansx && ansx <= sx0) ) ) {
        //Inside the line segment.
        if (fxy) {
            //Deswapify them!
            T temp = ansx;
            ansx = ansy;
            ansy = temp;
        }
        return true;
    }
    //Too bad. They are outside the line segment
    return false;
}

void NavigationSystem::IntersectBorder( float &x, float &y, const float &x1, const float &y1 ) const
{
    float ansx;
    float ansy;
    if ( intersect( x, y, x1, y1, screenskipby4[1], screenskipby4[3], screenskipby4[0], screenskipby4[3], ansx, ansy )
        || intersect( x, y, x1, y1, screenskipby4[0], screenskipby4[2], screenskipby4[0], screenskipby4[3], ansx, ansy )
        || intersect( x, y, x1, y1, screenskipby4[0], screenskipby4[2], screenskipby4[1], screenskipby4[2], ansx, ansy )
        || intersect( x, y, x1, y1, screenskipby4[1], screenskipby4[3], screenskipby4[1], screenskipby4[2], ansx, ansy ) ) {
        x = ansx;
        y = ansy;
    }
}

//tests if given are in the range
//**********************************
bool NavigationSystem::TestIfInRange( float &x1, float &x2, float &y1, float &y2, float tx, float ty )
{
    if ( ( (tx < x2) && (tx > x1) ) && ( (ty < y2) && (ty > y1) ) )
        return 1;
    else
        return 0;
}
//**********************************

//tests if given are in the circle range
//**********************************
bool NavigationSystem::TestIfInRangeRad( float &x, float &y, float size, float tx, float ty )
{
    if ( ( ( (x-tx)*(x-tx) )+( (y-ty)*(y-ty) ) ) < ( (0.5*size)*(0.5*size) ) )
        return 1;
    else
        return 0;
}

//**********************************

//Tests if given are in block range
//**********************************
bool NavigationSystem::TestIfInRangeBlk( float &x, float &y, float size, float tx, float ty )
{
    if ( ( Delta( tx, x ) < (0.5*size) ) && ( Delta( ty, y ) < (0.5*size) ) )
        return 1;
    else
        return 0;
}
//**********************************

/*
 *  //	Gived the delta of 2 items
 *  //	**********************************
 *  float NavigationSystem::Delta(float a, float b)
 *  {
 *
 *       float ans = a-b;
 *       if(ans < 0)
 *               return (-1.0 * ans);
 *       else
 *               return ans;
 *  }
 *  //	**********************************
 */

//converts the % of screen system to 0-center system
//**********************************
void NavigationSystem::ScreenToCoord( float &x )
{
    x -= .5;
    x *= 2;
}
//**********************************

//checks if the draw flag is 1
//**********************************
bool NavigationSystem::CheckDraw()
{
    return draw == 1;
}
//**********************************

void NavigationSystem::Adjust3dTransformation( bool system_vs_galaxy )
{
    // Coherent camera model input handler. Drives the active view's NavMap
    // directly, with consistent bindings across the whole nav computer:
    //   right-drag  = rotate (orbit in 3D, roll the map in top-down)
    //   left-drag   = pan (translate the camera perpendicular to the view)
    //   middle-drag = pan
    //   wheel       = zoom (move the camera forward/back toward the focus)
    NavMap &cam = system_vs_galaxy ? systemCam : galaxyCam;
    if ( !TestIfInRange( screenskipby4[0], screenskipby4[1], screenskipby4[2], screenskipby4[3], mouse_x_current,
                         mouse_y_current ) )
        return;

    // ROTATE on right (button 3) drag — orbit (3D) or roll (top-down).
    // Sensitivity tuned for the normalised mouse deltas (~[-1,1] across the
    // screen): 0.6 rad per full-screen drag (3x the 0.2 baseline, per play-test).
    if ( mouse_previous_state[2] == 1 ) {
        float ndx = (mouse_x_current-mouse_x_previous);
        float ndy = (mouse_y_current-mouse_y_previous);
        cam.orbitBy( ndx*0.6f, -ndy*0.6f );   // y flipped so drag-up looks up
    }

    // PAN on left (button 1) or middle (button 2) drag — strafe the camera
    // along its right/up axes. Step scales with the distance to the nearest
    // significant object so panning is fast in empty space and fine near objects.
    if ( (mouse_previous_state[0] == 1) || (mouse_previous_state[1] == 1) ) {
        float ndx = (mouse_x_current-mouse_x_previous);
        float ndy = (mouse_y_current-mouse_y_previous);
        double step = navNearDist * 0.5;
        cam.panBy( -ndx*step, -ndy*step );
    }

    // ZOOM on wheel — move the camera forward (wheel up) or back (wheel down)
    // along its view direction. Step scales with navNearDist: huge through
    // empty space, fine as you close on an object.
    int  wheelbits = getMouseButtonStatus();
    bool wheelup   = (wheelbits & 8) != 0;
    bool wheeldn   = (wheelbits & 16) != 0;
    static float wheel_zoom_amount = XMLSupport::parse_float( vs_config->getVariable( "graphics", "wheel_zoom_amount", "0.1" ) );
    if ( wheelup || wheeldn || mouse_wentdown[3] || mouse_wentdown[4] ) {
        double step = navNearDist * wheel_zoom_amount;
        cam.zoomBy( (wheelup || mouse_wentdown[3]) ? step : -step );
    }
    //**********************************
}

void NavigationSystem::arrowKey( int dir, unsigned int mods )
{
    // Keyboard camera control, active only while the nav is open (the caller
    // gates the ship handlers). Drives the active view's camera.
    //   plain          = pan (strafe the camera right/up)
    //   Shift          = rotate (yaw/pitch, look around)
    //   Alt+up/down    = zoom forward/back
    //   Alt+left/right = pan sideways
    // Step scales with navNearDist so movement is fast in empty space and fine
    // near objects, matching the mouse.
    NavMap &cam = checkbit( whattodraw, 2 ) ? galaxyCam : systemCam;

    bool shift = (mods & KB_MOD_SHIFT) != 0;
    bool alt   = (mods & KB_MOD_ALT)   != 0;

    // Up/down (dir 0/1) vs left/right (dir 2/3).
    double step = navNearDist * 0.5;
    if (shift) {
        // Rotate: up/down = pitch, left/right = yaw.
        float amt = 0.05f;
        if (dir == 0)      cam.orbitBy( 0,  amt );      // up   = look up
        else if (dir == 1) cam.orbitBy( 0, -amt );      // down = look down
        else if (dir == 2) cam.orbitBy( amt, 0 );       // left = look left
        else               cam.orbitBy( -amt, 0 );      // right= look right
    } else if (alt) {
        if (dir == 0)      cam.zoomBy(  step );          // Alt+up   = zoom in
        else if (dir == 1) cam.zoomBy( -step );          // Alt+down = zoom out
        else if (dir == 2) cam.panBy( -step, 0 );        // Alt+left = pan left
        else               cam.panBy(  step, 0 );        // Alt+right= pan right
    } else {
        // Pan (strafe the camera). left/right along camera right; up/down along
        // camera up.
        if (dir == 0)      cam.panBy( 0,  step );        // up   = move up
        else if (dir == 1) cam.panBy( 0, -step );        // down = move down
        else if (dir == 2) cam.panBy( -step, 0 );        // left = move left
        else               cam.panBy(  step, 0 );        // right= move right
    }
}

void NavigationSystem::RecordMinAndMax( const QVector &pos,
                                        float &min_x,
                                        float &max_x,
                                        float &min_y,
                                        float &max_y,
                                        float &min_z,
                                        float &max_z,
                                        float &max_all )
{
    //Record min and max
    //**********************************
    if ( (float) pos.i > max_x )
        max_x = (float) pos.i;
    if ( (float) pos.i < min_x )
        min_x = (float) pos.i;
//if( fabs((float)pos.i) > max_all )
//max_all = fabs((float)pos.i);
    if ( ( fabs( max_x-min_x ) ) > max_all )
        max_all = 0.5*( fabs( max_x-min_x ) );
    if ( (float) pos.j > max_y )
        max_y = (float) pos.j;
    if ( (float) pos.j < min_y )
        min_y = (float) pos.j;
//if( fabs((float)pos.j) > max_all )
//max_all = fabs((float)pos.j);
    if ( ( fabs( max_y-min_y ) ) > max_all )
        max_all = 0.5*( fabs( max_y-min_y ) );
    if ( (float) pos.k > max_z )
        max_z = (float) pos.k;
    if ( (float) pos.k < min_z )
        min_z = (float) pos.k;
//if( fabs((float)pos.k) > max_all )
//max_all = fabs((float)pos.k);
    if ( ( fabs( max_z-min_z ) ) > max_all )
        max_all = 0.5*( fabs( max_z-min_z ) );
    //**********************************
}

void NavigationSystem::DrawOriginOrientationTri( float center_nav_x, float center_nav_y, bool system_not_galaxy )
{
    // Draw the world X/Y/Z orientation triad at the camera's view focus point
    // (position + view direction * nominal distance), projected through the
    // camera. Each axis is drawn from the projected focus to the projected tip.
    NavMap &cam = system_not_galaxy ? systemCam : galaxyCam;
    float   len = 0.25f * cam.nominalDistance();
    // Focus point = camera position + view direction * nominal distance.
    QVector center = cam.position() + cam.forward() * cam.nominalDistance();

    float cx, cy, css, ax, ay, ass;
    if ( !cam.project( center, cx, cy, css ) )
        return;
    float ox = center_nav_x + cx;
    float oy = center_nav_y + cy;

    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    // X=red, Y=green, Z=blue
    const QVector dirs[3] = { QVector( 1, 0, 0 ), QVector( 0, 1, 0 ), QVector( 0, 0, 1 ) };
    const float  cols[3][4] = { { 1, 0, 0, 0.5f }, { 0, 1, 0, 0.5f }, { 0, 0, 1, 0.5f } };
    float verts[3*2*(3+4)];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        if ( cam.project( center + dirs[i]*len, ax, ay, ass ) ) {
            verts[n*7+0] = ox;              verts[n*7+1] = oy;              verts[n*7+2] = 0;
            verts[n*7+3] = cols[i][0];      verts[n*7+4] = cols[i][1];      verts[n*7+5] = cols[i][2]; verts[n*7+6] = cols[i][3];
            ++n;
            verts[n*7+0] = center_nav_x+ax; verts[n*7+1] = center_nav_y+ay; verts[n*7+2] = 0;
            verts[n*7+3] = cols[i][0];      verts[n*7+4] = cols[i][1];      verts[n*7+5] = cols[i][2]; verts[n*7+6] = cols[i][3];
            ++n;
        }
    }
    if (n > 0)
        GFXDraw( GFXLINE, verts, n, 3, 4 );

    GFXEnable( TEXTURE0 );
    //**********************************
}

void Beautify( string systemfile, string &sector, string &system )
{
    string::size_type slash = systemfile.find( "/" );
    if (slash == string::npos) {
        sector = "";
        system = systemfile;
    } else {
        sector = systemfile.substr( 0, slash );
        system = systemfile.substr( slash+1 );
    }
    if ( sector.size() )
        sector[0] = toupper( sector[0] );
    if ( system.size() )
        system[0] = toupper( system[0] );
}

