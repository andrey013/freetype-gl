/* =========================================================================
 * Freetype GL - A C OpenGL Freetype engine
 * Platform:    Any
 * WWW:         http://code.google.com/p/freetype-gl/
 * -------------------------------------------------------------------------
 * Copyright 2011 Nicolas P. Rougier. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NICOLAS P. ROUGIER ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL NICOLAS P. ROUGIER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Nicolas P. Rougier.
 * ========================================================================= */
#include <AntTweakBar.h>
#if defined(__APPLE__)
    #include <Glut/glut.h>
#else
    #include <GL/glut.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <math.h>
#include "vector.h"
#include "texture-font.h"
#include "texture-atlas.h"
#include "vertex-buffer.h"

// ------------------------------------------------------- typedef & struct ---
typedef enum {
    GEORGIA=1,
    TIMES,
    VERDANA,
    TAHOMA,
    ARIAL,
} Family;

typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
    float m;
} GlyphVertex;

#define NUM_FONTS 5

// ------------------------------------------------------- global variables ---
TwBar *bar;
VertexBuffer *buffer;
TextureAtlas *atlas_gray, *atlas_rgb, *atlas;
GLuint program = 0;
GLuint texture_location;
GLuint pixel_location;
GLuint gamma_location;
GLuint primary_location;
GLuint secondary_location;
GLuint tertiary_location;

Family p_family;
float p_size;
int p_invert;
int p_kerning;
int p_hinting;
int p_lcd_filtering;
float p_gamma;
float p_interval;
float p_weight;
float p_width;
float p_faux_weight;
float p_faux_italic;
float p_primary;
float p_secondary;
float p_tertiary;

static wchar_t text[] = 
    L"A single pixel on a color LCD is made of three colored elements \n"
    L"ordered (on various displays) either as blue, green, and red (BGR), \n"
    L"or as red, green, and blue (RGB). These pixel components, sometimes \n"
    L"called sub-pixels, appear as a single color to the human eye because \n"
    L"of blurring by the optics and spatial integration by nerve cells in "
    L"the eye.\n"
    L"\n"
    L"The resolution at which colored sub-pixels go unnoticed differs, \n"
    L"however, with each user some users are distracted by the colored \n"
    L"\"fringes\" resulting from sub-pixel rendering. Subpixel rendering \n"
    L"is better suited to some display technologies than others. The \n"
    L"technology is well-suited to LCDs, but less so for CRTs. In a CRT \n"
    L"the light from the pixel components often spread across pixels, \n"
    L"and the outputs of adjacent pixels are not perfectly independent."
    L"\n";


// ------------------------------------------------------------ read_shader ---
char *
read_shader( const char *filename )
{
    FILE * file;
    char * buffer;
	size_t size;

    file = fopen( filename, "rb" );
    if( !file )
    {
        fprintf( stderr, "Unable to open file \"%s\".\n", filename );
		return 0;
    }
	fseek( file, 0, SEEK_END );
	size = ftell( file );
	fseek(file, 0, SEEK_SET );
    buffer = (char *) malloc( (size+1) * sizeof( char *) );
	fread( buffer, sizeof(char), size, file );
    buffer[size] = 0;
    fclose( file );
    return buffer;
}

// ----------------------------------------------------------- build_shader ---
GLuint
build_shader( const char* source, GLenum type )
{
    GLuint handle = glCreateShader( type );
    glShaderSource( handle, 1, &source, 0 );
    glCompileShader( handle );

    GLint compile_status;
    glGetShaderiv( handle, GL_COMPILE_STATUS, &compile_status );
    if( compile_status == GL_FALSE )
    {
        GLchar messages[256];
        glGetShaderInfoLog( handle, sizeof(messages), 0, &messages[0] );
        fprintf( stderr, "%s\n", messages );
        exit( EXIT_FAILURE );
    }
    return handle;
}

// ---------------------------------------------------------- build_program ---
GLuint build_program( const char * vertex_source,
                      const char * fragment_source )
{
    GLuint vertex_shader   = build_shader( vertex_source, GL_VERTEX_SHADER);
    GLuint fragment_shader = build_shader( fragment_source, GL_FRAGMENT_SHADER);
    
    GLuint handle = glCreateProgram( );
    glAttachShader( handle, vertex_shader);
    glAttachShader( handle, fragment_shader);
    glLinkProgram( handle);
    
    GLint link_status;
    glGetProgramiv( handle, GL_LINK_STATUS, &link_status );
    if (link_status == GL_FALSE)
    {
        GLchar messages[256];
        glGetProgramInfoLog( handle, sizeof(messages), 0, &messages[0] );
        fprintf( stderr, "%s\n", messages );
        exit(1);
    }
    
    return handle;
}


// -------------------------------------------------------------- add_glyph ---
void add_glyph( const TextureGlyph *glyph,
                VertexBuffer *buffer,
                const Markup *markup,
                Pen *pen,
                float kerning )
{
    if( p_kerning )
    {
        pen->x += kerning;
    }
    else
    {
    }

    float r = markup->foreground_color.r;
    float g = markup->foreground_color.g;
    float b = markup->foreground_color.b;
    float a = markup->foreground_color.a;
    float x0  = ( pen->x + glyph->offset_x );
    float y0  = (int)( pen->y + glyph->offset_y );
    float x1  = ( x0 + glyph->width * p_width );
    float y1  = (int)( y0 - glyph->height );

    float u0 = glyph->u0;
    float v0 = glyph->v0;
    float u1 = glyph->u1;
    float v1 = glyph->v1;
    GLuint index = buffer->vertices->size;
    GLuint indices[] = {index, index+1, index+2,
                        index, index+2, index+3};

    float dx = sin(p_faux_italic * M_PI/6) * glyph->height;

    GlyphVertex vertices[] = { { (int)x0+dx,y0,0,  u0,v0,  r,g,b,a,  x0+dx-((int)(x0+dx)) },
                               { (int)x0,   y1,0,  u0,v1,  r,g,b,a,  x0-((int)x0) },
                               { (int)x1,   y1,0,  u1,v1,  r,g,b,a,  x1-((int)x1) },
                               { (int)x1+dx,y0,0,  u1,v0,  r,g,b,a,  x1+dx-((int)(x1+dx)) } };
    vertex_buffer_push_back_indices( buffer, indices, 6 );
    vertex_buffer_push_back_vertices( buffer, vertices, 4 );
    pen->x += glyph->advance_x * (1.0 + p_interval);
    pen->y += glyph->advance_y;
}

// ----------------------------------------------------------- build_buffer ---
void
build_buffer( void )
{ 
    Pen pen;
    size_t i;
    TextureFont *font;
    TextureGlyph *glyph;
    Markup markup = { "Arial", 10, 0, 0, 0.0, 0.0,
                      {1,1,1,1}, {0,0,0,0},
                      0, {0,0,0,1}, 0, {0,0,0,1},
                      0, {0,0,0,1}, 0, {0,0,0,1}, 0 };

    vertex_buffer_clear( buffer );
    texture_atlas_clear( atlas );

    if( p_family == GEORGIA)
    {
        font = texture_font_new( atlas, "Georgia.ttf", p_size );
    }
    else if( p_family == TIMES )
    {
        font = texture_font_new( atlas, "Times.ttf", p_size );
    }
    else if( p_family == TAHOMA )
    {
        font = texture_font_new( atlas, "Tahoma.ttf", p_size );
    }
    else if( p_family == ARIAL )
    {
        font = texture_font_new( atlas, "Arial.ttf", p_size );
    }
    else if( p_family == VERDANA )
    {
        font = texture_font_new( atlas, "Verdana.ttf", p_size );
    }
    else
    {
        fprintf( stderr, "Error : Unknonw family type\n" );
        return;
    }

    font->hinting = p_hinting;
    font->lcd_filter = 1;
    float norm = 1.0/(p_primary + 2*p_secondary + 2*p_tertiary);
    font->lcd_weights[0] = (unsigned char)(p_tertiary*norm*256);
    font->lcd_weights[1] = (unsigned char)(p_secondary*norm*256);
    font->lcd_weights[2] = (unsigned char)(p_primary*norm*256);
    font->lcd_weights[3] = (unsigned char)(p_secondary*norm*256);
    font->lcd_weights[4] = (unsigned char)(p_tertiary*norm*256);

    texture_font_cache_glyphs( font, 
                               L" !\"#$%&'()*+,-./0123456789:;<=>?"
                               L"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                               L"`abcdefghijklmnopqrstuvwxyz{|}~" );
    pen.x = 10;
    pen.y = 600 - font->height - 10;

    glyph = texture_font_get_glyph( font, text[0] );
    add_glyph( glyph, buffer, &markup, &pen, 0 );
    for( i=1; i<wcslen(text); ++i )
    {
        if( text[i] == '\n' )
        {
            pen.x  = 10;
            pen.y -= font->height; // + 0.01*(size - (int)size)*font->height;
        }
        else
        {
            glyph = texture_font_get_glyph( font, text[i] );
            float kerning = 0.0;
            if( p_kerning )
            {
                kerning = texture_glyph_get_kerning( glyph, text[i-1] );
            }
            add_glyph( glyph, buffer, &markup, &pen, kerning );
        }
    }
}



// ---------------------------------------------------------------- display ---
void display(void)
{
    if( p_invert )
    {
        glClearColor( 0, 0, 0, 1 );
    }
    else
    {
        glClearColor( 1, 1, 1, 1 );
    }
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, atlas->texid );
    if( !p_lcd_filtering )
    {
        glEnable( GL_COLOR_MATERIAL );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        if( p_invert )
        {
            glColor4f(1,1,1,1);
        }
        else
        {
            glColor4f(0,0,0,1);
        }
    }
    else
    {
        glEnable( GL_COLOR_MATERIAL );
        glBlendFunc( GL_CONSTANT_COLOR_EXT,
                     GL_ONE_MINUS_SRC_COLOR );
        glEnable( GL_BLEND );
        glColor3f( 1,1,1 );
        if( p_invert )
        {
            glBlendColor( 1, 1, 1, 1 );
        }
        else
        {
            glBlendColor( 0, 0, 0, 1 );
        }
    }

    if( !p_lcd_filtering )
    {
        vertex_buffer_render( buffer, GL_TRIANGLES, "vt" );
    }
    else
    {
        glUseProgram( program );
        glUniform1i( texture_location, 0 );
        glUniform1f( gamma_location, p_gamma );

        float norm = 1.0/(p_primary+2*p_secondary+2*p_tertiary);
        glUniform1f( primary_location,   p_primary*norm );
        glUniform1f( secondary_location, p_secondary*norm );
        glUniform1f( tertiary_location,  p_tertiary*norm );
        glUniform2f( pixel_location,
                     1.0/atlas->width,
                     1.0/atlas->height );
        vertex_buffer_render( buffer, GL_TRIANGLES, "vtc" );
        glUseProgram( 0 );
    }

    TwDraw( );
    glutSwapBuffers( );
}


// ---------------------------------------------------------------- reshape ---
void reshape( int width, int height )
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glutPostRedisplay();
    TwWindowSize( width, height );
}


// ------------------------------------------------------------------- quit ---
void reset( void )
{ 
    p_family    = GEORGIA;
    p_size      = 16.0;
    p_invert    = 0;
    p_kerning   = 1;
    p_hinting   = 1;
    p_lcd_filtering = 1;
    p_gamma     = 1.0;
    p_interval  = 0.0;
    p_weight    = 0.33;
    p_width     = 1.0;
    p_faux_weight = 0.0;
    p_faux_italic = 0.0;

    // FT_LCD_FILTER_LIGHT
    p_primary   = 1.0/3.0;
    p_secondary = 1.0/3.0;
    p_tertiary  = 0.0/3.0;

    // FT_LCD_FILTER_DEFAULT
    // p_primary   = 3.0/9.0;
    // p_secondary = 2.0/9.0;
    // p_tertiary  = 1.0/9.0;

    if( !p_lcd_filtering )
    {
        atlas = atlas_gray;
    }
    else
    {
        atlas = atlas_rgb;
    }
    build_buffer();
}

// ------------------------------------------------------------------- quit ---
void quit( void )
{ 
    exit( EXIT_SUCCESS );
}

// -------------------------------------------------------------- terminate ---
void terminate( void )
{ 
    TwTerminate();
}

void timer( int fps )
{
    glutPostRedisplay();
    glutTimerFunc( 1000.0/fps, timer, fps );
}

// --------------------------------------------------------- get/set invert ---
void TW_CALL set_invert( const void *value, void *data )
{
    p_invert = *(const int *) value;
}
void TW_CALL get_invert( void *value, void *data )
{
    *(int *)value = p_invert;
}

// -------------------------------------------------------- get/set kerning ---
void TW_CALL set_kerning( const void *value, void *data )
{
    p_kerning = *(const int *) value;
    build_buffer();
}
void TW_CALL get_kerning( void *value, void *data )
{
    *(int *)value = p_kerning;
}

// -------------------------------------------------------- get/set hinting ---
void TW_CALL set_hinting( const void *value, void *data )
{
    p_hinting = *(const int *) value;
    build_buffer();
}
void TW_CALL get_hinting( void *value, void *data )
{
    *(int *)value = p_hinting;
}

// -------------------------------------------------- get/set lcd_filtering ---
void TW_CALL set_lcd_filtering( const void *value, void *data )
{
    p_lcd_filtering = *(const int *) value;
    if( p_lcd_filtering )
    {
        atlas = atlas_rgb;
    }
    else
    {
        atlas = atlas_gray;
    }
    build_buffer();
}
void TW_CALL get_lcd_filtering( void *value, void *data )
{
    *(int *)value = p_lcd_filtering;
}

// --------------------------------------------------------- get/set weight ---
void TW_CALL set_weight( const void *value, void *data )
{
    p_weight = *(const float *) value;
}
void TW_CALL get_weight( void *value, void *data )
{
    *(float *)value = p_weight;
}

// ---------------------------------------------------------- get/set gamma ---
void TW_CALL set_gamma( const void *value, void *data )
{
    p_gamma = *(const float *) value;
}
void TW_CALL get_gamma( void *value, void *data )
{
   *(float *)value = p_gamma;
}

// ---------------------------------------------------------- get/set width ---
void TW_CALL set_width( const void *value, void *data )
{
    p_width = *(const float *) value;
    build_buffer();
}
void TW_CALL get_width( void *value, void *data )
{
    *(float *)value = p_width;
}

// ------------------------------------------------------- get/set interval ---
void TW_CALL set_interval( const void *value, void *data )
{
    p_interval = *(const float *) value;
    build_buffer();
}
void TW_CALL get_interval( void *value, void *data )
{
    *(float *)value = p_interval;
}

// ---------------------------------------------------- get/set faux_weight ---
void TW_CALL set_faux_weight( const void *value, void *data )
{
    p_faux_weight = *(const float *) value;
}
void TW_CALL get_faux_weight( void *value, void *data )
{
    *(float *)value = p_faux_weight;
}

// ---------------------------------------------------- get/set faux_italic ---
void TW_CALL set_faux_italic( const void *value, void *data )
{
    p_faux_italic = *(const float *) value;
    build_buffer();
}
void TW_CALL get_faux_italic( void *value, void *data )
{
    *(float *)value = p_faux_italic;
}

// ----------------------------------------------------------- get/set size ---
void TW_CALL set_size( const void *value, void *data )
{
    p_size = *(const float *) value;
    build_buffer();

}
void TW_CALL get_size( void *value, void *data )
{
    *(float *)value = p_size;
}

// --------------------------------------------------------- get/set family ---
void TW_CALL set_family( const void *value, void *data )
{
    p_family = *(const Family *) value;
    build_buffer();
}
void TW_CALL get_family( void *value, void *data )
{
    *(Family *)value = p_family;
}

// ----------------------------------------------------------- get/set primary ---
void TW_CALL set_primary( const void *value, void *data )
{
    p_primary = *(const float *) value;
    build_buffer();

}
void TW_CALL get_primary( void *value, void *data )
{
    *(float *)value = p_primary;
}

// ----------------------------------------------------------- get/set secondary ---
void TW_CALL set_secondary( const void *value, void *data )
{
    p_secondary = *(const float *) value;
    build_buffer();

}
void TW_CALL get_secondary( void *value, void *data )
{
    *(float *)value = p_secondary;
}

// ----------------------------------------------------------- get/set tertiary ---
void TW_CALL set_tertiary( const void *value, void *data )
{
    p_tertiary = *(const float *) value;
    build_buffer();

}
void TW_CALL get_tertiary( void *value, void *data )
{
    *(float *)value = p_tertiary;
}
 

// Main
int main(int argc, char *argv[])
{
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowSize( 800, 600 );
    glutCreateWindow( "Font rendering advanced tweaking" );
    glutCreateMenu( NULL );


    glutDisplayFunc( display );
    glutReshapeFunc( reshape );
    atexit( terminate );
    TwInit( TW_OPENGL, NULL );
    glutMouseFunc( (GLUTmousebuttonfun)TwEventMouseButtonGLUT );
    glutMotionFunc( (GLUTmousemotionfun)TwEventMouseMotionGLUT );
    glutPassiveMotionFunc( (GLUTmousemotionfun)TwEventMouseMotionGLUT );
    glutKeyboardFunc( (GLUTkeyboardfun)TwEventKeyboardGLUT );
    glutSpecialFunc( (GLUTspecialfun)TwEventSpecialGLUT );
    TwGLUTModifiersFunc( glutGetModifiers );

    // Create a new tweak bar
    bar = TwNewBar("TweakBar");
    TwDefine("GLOBAL "
             "help = 'This example shows how to tune all font parameters.' ");
    TwDefine("TweakBar                      "
             "size          = '280 400'     "
             "position      = '500 20'      "
             "color         = '127 127 127' "
             "alpha         = 240           "
             "label         = 'Parameters'  "
             "resizable     = True          "
             "fontresizable = True          "
             "iconifiable   = True          ");

    {
        TwEnumVal familyEV[NUM_FONTS] = { {GEORGIA, "Georgia"},
                                          {TIMES,   "Times"},
                                          {VERDANA, "Verdana"},
                                          {TAHOMA,  "Tahoma"},
                                          {ARIAL,   "Arial"} };
        TwType family_type = TwDefineEnum("Family", familyEV, NUM_FONTS);
        TwAddVarCB(bar, "Family", family_type, set_family, get_family, NULL, 
                   "label = 'Family'      "
                   "group = 'Font'        "
                   "help  = ' '           ");
    }
    TwAddVarCB(bar, "Size", TW_TYPE_FLOAT, set_size, get_size, NULL, 
               "label = 'Size' "
               "group = 'Font' "
               "min   = 6.0    "
               "max   = 24.0   "
               "step  = 0.05   "
               "help  = ' '    ");
    TwAddVarCB(bar, "LCD filtering", TW_TYPE_BOOL32, set_lcd_filtering, get_lcd_filtering, NULL, 
               "label = 'LCD filtering' "
              "group = 'Font'        "
               "help  = ' '             ");


    // Rendering
    TwAddVarCB(bar, "Kerning", TW_TYPE_BOOL32, set_kerning, get_kerning, NULL, 
               "label = 'Kerning'   "
               "group = 'Rendering' "
               "help  = ' '         ");
    TwAddVarCB(bar, "Hinting", TW_TYPE_BOOL32, set_hinting, get_hinting, NULL, 
               "label = 'Hinting'   "
               "group = 'Rendering' "
               "help  = ' '         ");

    // Color
    TwAddVarCB(bar, "Invert", TW_TYPE_BOOL32, set_invert, get_invert, NULL, 
               "label = 'Invert' "
               "group = 'Color'  "
               "help  = ' '      ");
    
    // Glyph
    TwAddVarCB(bar, "Width", TW_TYPE_FLOAT, set_width, get_width, NULL, 
               "label = 'Width' "
               "group = 'Glyph' "
               "min   = 0.75    "
               "max   = 1.25    " 
               "step  = 0.01    "
               "help  = ' '     ");

    TwAddVarCB(bar, "Interval", TW_TYPE_FLOAT, set_interval, get_interval, NULL, 
               "label = 'Spacing' "
               "group = 'Glyph'   "
               "min   = -0.2      "
               "max   = 0.2       "
               "step  = 0.01      "
               "help  = ' '       " );

    TwAddVarCB(bar, "Faux italic", TW_TYPE_FLOAT, set_faux_italic, get_faux_italic, NULL, 
               "label = 'Faux italic' "
               "group = 'Glyph'       "
               "min   = -1.0          "
               "max   = 1.0           "
               "step  = 0.01          "
               "help  = ' '           ");

    // Energy distribution
    TwAddVarCB(bar, "Primary", TW_TYPE_FLOAT, set_primary, get_primary, NULL,
               "label = 'Primary weight'      "
               "group = 'Energy distribution' "
               "min   = 0                     "
               "max   = 1                     "
               "step  = 0.01                  "
               "help  = ' '                   " );

    TwAddVarCB(bar, "Secondary", TW_TYPE_FLOAT, set_secondary, get_secondary, NULL,
               "label = 'Secondy weight'      "
               "group = 'Energy distribution' "
               "min   = 0                     "
               "max   = 1                     "
               "step  = 0.01                  "
               "help  = ' '                   " );

    TwAddVarCB(bar, "Tertiary", TW_TYPE_FLOAT, set_tertiary, get_tertiary, NULL,
               "label = 'Tertiary weight'      "
               "group = 'Energy distribution' "
               "min   = 0                     "
               "max   = 1                     "
               "step  = 0.01                  "
               "help  = ' '                   " );

    TwAddSeparator(bar, "",
                   "group = 'Energy distribution' " );

    TwAddVarCB(bar, "Gamma", TW_TYPE_FLOAT, set_gamma, get_gamma, NULL, 
               "label = 'Gamma correction'    "
               "group = 'Energy distribution' "
               "min   = 0.50                  "
               "max   = 2.5                   "
               "step  = 0.01                  "
               "help  = ' '                   ");

    TwAddSeparator(bar, "", "");
    TwAddButton(bar, "Reset", (TwButtonCallback) reset, NULL,
                "help='Reset all parameters to default values.'");
    TwAddSeparator(bar, "", "");
    TwAddButton(bar, "Quit", (TwButtonCallback) quit, NULL,
                "help='Quit.'");

    atlas_gray = texture_atlas_new( 512, 256, 1 );
    atlas_rgb  = texture_atlas_new( 512, 256, 3 );
    buffer = vertex_buffer_new( "v3f:t2f:c4f:1g1f" ); 
    reset();

    // Create the GLSL program
    char * vertex_shader_source   = read_shader("./subpixel.vert");
    char * fragment_shader_source = read_shader("./subpixel-gamma.frag");
    program = build_program( vertex_shader_source, fragment_shader_source );
    texture_location = glGetUniformLocation( program, "texture" );
    pixel_location = glGetUniformLocation( program, "pixel" );
    gamma_location = glGetUniformLocation( program, "gamma" );
    primary_location   = glGetUniformLocation( program, "primary" );
    secondary_location = glGetUniformLocation( program, "secondary" );
    tertiary_location  = glGetUniformLocation( program, "tertiary" );

    //glEnable(GL_FRAMEBUFFER_SRGB);
    glutTimerFunc( 1000.0/60.0, timer, 60 );
    glutMainLoop();
    return EXIT_SUCCESS;
}

