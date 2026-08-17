//This draws the mouse cursor
//**********************************
void NavigationSystem::DrawCursor( float x, float y, float wid, float hei, const GFXColor &col )
{
    float sizex, sizey;
    static bool modern_nav_cursor =
        XMLSupport::parse_bool( vs_config->getVariable( "graphics", "nav", "modern_mouse_cursor", "true" ) );
    if (modern_nav_cursor) {
        static string   mouse_cursor_sprite = vs_config->getVariable( "graphics", "nav", "mouse_cursor_sprite", "mouse.spr" );
        static VSSprite MouseVSSprite( mouse_cursor_sprite.c_str(), BILINEAR, GFXTRUE );
        GFXBlendMode( SRCALPHA, INVSRCALPHA );
        GFXColorf( GUI_OPAQUE_WHITE() );

        //Draw the cursor sprite.
        GFXEnable( TEXTURE0 );
        GFXDisable( DEPTHTEST );
        GFXDisable( TEXTURE1 );
        MouseVSSprite.GetSize( sizex, sizey );
        MouseVSSprite.SetPosition( x+sizex/2, y+sizey/2 );
        MouseVSSprite.Draw();
    } else {
        GFXColorf( col );
        GFXDisable( TEXTURE0 );
        GFXDisable( LIGHTING );
        GFXBlendMode( SRCALPHA, INVSRCALPHA );

        const float verts[8 * 3] = {
            x,          y,          0,
            x,          y-hei,      0,
            x,          y,          0,
            x+wid,      y-0.75f*hei, 0,
            x,          y-hei,      0,
            x+0.35f*wid, y-0.6f*hei,  0,
            x+0.35f*wid, y-0.6f*hei,  0,
            x+wid,      y-0.75f*hei, 0,
        };
        GFXDraw( GFXLINE, verts, 8 );

        GFXEnable( TEXTURE0 );
    }
}
//**********************************

//This draws the grid over the nav screen area
//**********************************
//This will draw a circle over the screen
//**********************************
void NavigationSystem::DrawCircle( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    // 20 segments
    static VertexBuilder<> verts;
    verts.clear();
    for ( float i = 0; i < 2*M_PI + M_PI/10; i += M_PI/10 ) {
        verts.insert(
            x+0.5*size*cos( i ),
            y+0.5*size*sin( i ),
            0
        );
    }
    GFXDraw( GFXLINESTRIP, verts );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a half circle, centered at the top 1/4 center
//**********************************
void NavigationSystem::DrawHalfCircleTop( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    // 10 segments
    static VertexBuilder<> verts;
    verts.clear();
    for (float i = 0; i < M_PI + M_PI/10; i += M_PI/10 ) {
        verts.insert(
            x+0.5*size*cos( i ),
            y+0.5*size*sin( i )-0.25*size,
            0
        );
    }
    GFXDraw( GFXLINESTRIP, verts );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a half circle, centered at the bottom 1/4 center
//**********************************
void NavigationSystem::DrawHalfCircleBottom( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    // 10 segments
    static VertexBuilder<> verts;
    verts.clear();
    for (float i = M_PI; i < 2*M_PI + M_PI/10; i += M_PI/10 ) {
        verts.insert(
            x+0.5*size*cos( i ),
            y+0.5*size*sin( i )+0.25*size,
            0
        );
    }
    GFXDraw( GFXLINESTRIP, verts );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a planet icon. circle + lightning thingy
//**********************************
void NavigationSystem::DrawPlanet( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    static VertexBuilder<> verts;
    verts.clear();
    for ( float i = 0; i < 2*M_PI; i += M_PI/10 ) {
        verts.insert(
            x+0.5*size*cos( i ),
            y+0.5*size*sin( i ),
            0
        );
        verts.insert(
            x+0.5*size*cos( i+M_PI/10 ),
            y+0.5*size*sin( i+M_PI/10 ),
            0
        );
    }
    verts.insert(x-0.5*size     , y             , 0 );
    verts.insert(x              , y+0.2*size    , 0 );
    verts.insert(x              , y+0.2*size    , 0 );
    verts.insert(x              , y-0.2*size    , 0 );
    verts.insert(x              , y-0.2*size    , 0 );
    verts.insert(x+0.5*size     , y             , 0 );
    GFXDraw( GFXLINE, verts );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a station icon. 3x3 grid
//**********************************
void NavigationSystem::DrawStation( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    float segment = size/3;
    static VertexBuilder<> verts;
    verts.clear();
    for (int i = 0; i < 4; i++) {
        verts.insert(
            x-0.5*size,
            y-0.5*size+i*segment,
            0
        );
        verts.insert(
            x+0.5*size,
            y-0.5*size+i*segment,
            0
        );
    }
    for (int i = 0; i < 4; i++) {
        verts.insert(
            x-0.5*size+i*segment,
            y-0.5*size,
            0
        );
        verts.insert(
            x-0.5*size+i*segment,
            y+0.5*size,
            0
        );
    }
    GFXDraw( GFXLINE, verts );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a jump node icon
//**********************************
void NavigationSystem::DrawJump( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    static VertexBuilder<> verts;
    verts.clear();
    for ( float i = 0; i < 2*M_PI; i += M_PI/10 ) {
        verts.insert(
            x+0.5*size*cos( i ),
            y+0.5*size*sin( i ),
            0
        );
        verts.insert(
            x+0.5*size*cos( i+M_PI/10 ),
            y+0.5*size*sin( i+M_PI/10 ),
            0
        );
    }
    verts.insert( x             , y+0.5*size    , 0 );
    verts.insert( x+0.125*size  , y+0.125*size  , 0 );
    verts.insert( x             , y+0.5*size    , 0 );
    verts.insert( x-0.125*size  , y+0.125*size  , 0 );
    verts.insert( x             , y-0.5*size    , 0 );
    verts.insert( x+0.125*size  , y-0.125*size  , 0 );
    verts.insert( x             , y-0.5*size    , 0 );
    verts.insert( x-0.125*size  , y-0.125*size  , 0 );
    verts.insert( x-0.5*size    , y             , 0 );
    verts.insert( x-0.125*size  , y+0.125*size  , 0 );
    verts.insert( x-0.5*size    , y             , 0 );
    verts.insert( x-0.125*size  , y-0.125*size  , 0 );
    verts.insert( x+0.5*size    , y             , 0 );
    verts.insert( x+0.125*size  , y+0.125*size  , 0 );
    verts.insert( x+0.5*size    , y             , 0 );
    verts.insert( x+0.125*size  , y-0.125*size  , 0 );
    GFXDraw( GFXLINE, verts );

    GFXEnable( TEXTURE0 );
}

//**********************************

//This will draw a missile icon
//**********************************
void NavigationSystem::DrawMissile( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    const float verts[12 * 3] = {
        x-0.5f*size,  y-0.125f*size, 0,
        x,           y+0.375f*size, 0,
        x+0.5f*size,  y-0.125f*size, 0,
        x,           y+0.375f*size, 0,
        x-0.25f*size, y-0.125f*size, 0,
        x-0.25f*size, y+0.125f*size, 0,
        x+0.25f*size, y-0.125f*size, 0,
        x+0.25f*size, y+0.125f*size, 0,
        x-0.25f*size, y+0.125f*size, 0,
        x,           y-0.125f*size, 0,
        x+0.25f*size, y+0.125f*size, 0,
        x,           y-0.125f*size, 0,
    };
    GFXDraw( GFXLINE, verts, 12 );

    GFXEnable( TEXTURE0 );
}
//**********************************

//This will draw a square set of corners
//**********************************
void NavigationSystem::DrawTargetCorners( float x, float y, float size, const GFXColor &col )
{
    GFXColorf( col );
    GFXDisable( TEXTURE0 );
    GFXDisable( LIGHTING );
    GFXBlendMode( SRCALPHA, INVSRCALPHA );

    const float verts[16 * 3] = {
        x-0.5f*size, y+0.5f*size, 0,
        x-0.3f*size, y+0.5f*size, 0,
        x-0.5f*size, y+0.5f*size, 0,
        x-0.5f*size, y+0.3f*size, 0,
        x+0.5f*size, y+0.5f*size, 0,
        x+0.3f*size, y+0.5f*size, 0,
        x+0.5f*size, y+0.5f*size, 0,
        x+0.5f*size, y+0.3f*size, 0,
        x-0.5f*size, y-0.5f*size, 0,
        x-0.3f*size, y-0.5f*size, 0,
        x-0.5f*size, y-0.5f*size, 0,
        x-0.5f*size, y-0.3f*size, 0,
        x+0.5f*size, y-0.5f*size, 0,
        x+0.3f*size, y-0.5f*size, 0,
        x+0.5f*size, y-0.5f*size, 0,
        x+0.5f*size, y-0.3f*size, 0,
    };
    GFXDraw( GFXLINE, verts, 16 );

    GFXEnable( TEXTURE0 );
}
//**********************************

