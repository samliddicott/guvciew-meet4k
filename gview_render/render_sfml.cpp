/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

#include "render_sfml.hpp"
#include <SFML/OpenGL.hpp>
#include <iostream>
#include <cstring>

extern "C" {
#include "gview.h"
#include "gviewrender.h"
#include "render.h"
#include "render_sfml.h"
#include "../config.h"
}

extern int verbosity;

/*
 * yu12 to rgba (rgb32)
 * args:
 *    out - pointer to output rgba data buffer
 *    in - pointer to input yu12 data buffer
 *    width - buffer width (in pixels)
 *    height - buffer height (in pixels)
 *
 * asserts:
 *    none
 *
 * returns: none
 */
static void yu12_to_rgba (uint8_t *out, uint8_t *in, int width, int height)
{
	uint8_t *py1 = in; //line 1
	uint8_t *py2 = py1 + width; //line 2
	uint8_t *pu = in + (width * height);
	uint8_t *pv = pu + ((width * height) / 4);
	
	uint8_t *pout1 = out; //first line
	uint8_t *pout2 = out + (width * 4); //second line
	
	int h=0, w=0;
	
	for(h=0; h < height; h+=2) //every two lines
	{
		py1 = in + (h * width);
		py2 = py1 + width;
		
		pout1 = out + (h * width * 4);
		pout2 = pout1 + (width * 4);
		
		for(w=0; w<width; w+=2) //every 2 pixels
		{
			/* standart: r = y0 + 1.402 (v-128) */
			/* logitech: r = y0 + 1.370705 (v-128) */
			*pout1++=CLIP(*py1 + 1.402 * (*pv-128));
			*pout2++=CLIP(*py2 + 1.402 * (*pv-128));	
			/* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
			/* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
			*pout1++=CLIP(*py1 - 0.34414 * (*pu-128) -0.71414*(*pv-128));
			*pout2++=CLIP(*py2 - 0.34414 * (*pu-128) -0.71414*(*pv-128));
			/* standart: b = y0 + 1.772 (u-128) */
			/* logitech: b = y0 + 1.732446 (u-128) */
			*pout1++=CLIP(*py1 + 1.772 *( *pu-128));
			*pout2++=CLIP(*py2 + 1.772 *( *pu-128));
			/* alpha set to 255*/
			*pout1++=255;
			*pout2++=255;

			py1++;
			py2++;
			
			/* standart: r1 =y1 + 1.402 (v-128) */
			/* logitech: r1 = y1 + 1.370705 (v-128) */
			*pout1++=CLIP(*py1 + 1.402 * (*pv-128));
			*pout2++=CLIP(*py2 + 1.402 * (*pv-128));
			/* standart: g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
			/* logitech: g1 = y1 - 0.337633 (u-128)- 0.698001 (v-128)*/
			*pout1++=CLIP(*py1 - 0.34414 * (*pu-128) -0.71414 * (*pv-128));
			*pout2++=CLIP(*py2 - 0.34414 * (*pu-128) -0.71414 * (*pv-128));
			/* standart: b1 = y1 + 1.772 (u-128) */
			/* logitech: b1 = y1 + 1.732446 (u-128) */
			*pout1++=CLIP(*py1 + 1.772 * (*pu-128));
			*pout2++=CLIP(*py2 + 1.772 * (*pu-128));
			/* alpha set to 255*/
			*pout1++=255;
			*pout2++=255;

			py1++;
			py2++;
			pu++;
			pv++;
		}
	}
}

SFMLRender::SFMLRender(int width, int height, int flags)
{
	int w = width;
	int h = height;
	
	use_shader = true;
	pix_buff = NULL;
	
	//get the current resolution
	sf::VideoMode display_mode = sf::VideoMode::getDesktopMode();

	if(verbosity > 0)
		std::cout << "RENDER: (SFML) desktop resolution (" <<
			display_mode.width << "x" << 
			display_mode.height << ")" << std::endl;

	if((display_mode.width > 0) && (display_mode.height > 0))
	{
		if(w > display_mode.width)
			w = display_mode.width;
		if(h > display_mode.height)
			h = display_mode.height;
	}

	// Create the main window
	if(flags == 1) //fullscreen
	{
		window.create(sf::VideoMode(w, h), "SFML window", sf::Style::Fullscreen);
		if(!window.isOpen())
		{
			std::cerr << "RENDER: (SFML) couldn't open window in fullscreen mode" << std::endl;
			window.create(sf::VideoMode(w, h), "SFML window");
		}
	}
	else
		window.create(sf::VideoMode(w, h), "SFML window");

	if(!window.isOpen())
	{
		std::cerr << "RENDER: (SFML) couldn't open window" << std::endl;
		return;
	}

	if(!texture.create(width, height))
	{
		std::cerr << "RENDER: (SFML) couldn't create texture" << std::endl;
		return;
	}

	if (sf::Shader::isAvailable() )
	{
		const std::string fragmentShader = \
			"uniform sampler2D texture;" \
			"void main(void)" \
			"{" \
				"float nx, ny, r, g, b, y, u, v;" \
				"float u1,u2,v1,v2;" \
				"nx = gl_TexCoord[0].x;" \
				"ny = gl_TexCoord[0].y;" \
				"y = texture2D(texture, vec2( (nx), (ny)*(4.0/6.0) )).r;" \
				"u1 = texture2D(texture, vec2( (nx/2.0), (ny+4.0)/6.0 )).r;" \
				"u2 = texture2D(texture, vec2( (nx/2.0)+0.5, (ny+4.0)/6.0 )).r;" \
				"v1 = texture2D(texture, vec2( (nx/2.0), (ny+5.0)/6.0 )).r;" \
				"v2 = texture2D(texture, vec2( (nx/2.0)+0.5, (ny+5.0)/6.0 )).r;" \
				"y = 1.1643 * (y - 0.0625);" \
				"u = (u1+u2)/2.0 - 0.5;" \
				"v = (v1+v2)/2.0 - 0.5;" \
				"r = y + 1.5958 * v;" \
				"g = y - 0.39173 * u - 0.8129 * v;" \
				"b = y + 2.017 * u;" \
				"gl_FragColor=vec4(b,g,r,1.0);" \
			"}";

		if(!conv_yuv2rgb_shd.loadFromMemory(fragmentShader, sf::Shader::Fragment))
		{
			std::cerr << "RENDER: (SFML) couldn't load fragment shader for yuv conversion" << std::endl;
			use_shader = false;
		}
		else
		{
			GLint textureBinding;
			int texWidth, texHeight;

			// Save the current texture binding, to avoid messing up SFML's OpenGL states
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);

			// Bind the texture
			sf::Texture::bind(&texture);

			// Get the actual texture size (can be different, if NPOT textures are not supported)
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);

			// Change its internal format (something with a single 8-bit component, whatever it is)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, texWidth, (texHeight*3/2), 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

			// Restore the previous texture binding
			glBindTexture(GL_TEXTURE_2D, textureBinding);

			conv_yuv2rgb_shd.setParameter("texture", sf::Shader::CurrentTexture);
		}
	}
	else
	{
		std::cerr << "RENDER: (SFML) shader not available for yuv conversion" << std::endl;
		use_shader = false;
	}
	
	texture.setSmooth(true);

	sprite.setTexture(texture);

	pix_buff = (uint8_t *) calloc(width*height*4, sizeof(uint8_t));

	window.clear(sf::Color::Black);
	window.display();

	if(verbosity > 1)
		std::cout << "RENDER: (SFML) window created" << std::endl;
}

SFMLRender::~SFMLRender()
{
	if(pix_buff)
		free(pix_buff);

	window.close();
}

int SFMLRender::render_frame(uint8_t *frame, int width, int height)
{
	//convert yuv to rgba
	
	if (use_shader)
	{
		memcpy((void *) pix_buff, frame, (width*height*3)/2);
		//update texture
		texture.update(pix_buff);
		//draw frame
		window.draw(sprite, &conv_yuv2rgb_shd);
	}
	else
	{
		yu12_to_rgba ((uint8_t *) pix_buff, frame, width, height);
		//update texture
		texture.update(pix_buff);
		//draw frame
		window.draw(sprite);
	}
	
	window.display();

	return 0;
}

void SFMLRender::set_caption(const char* caption)
{
	window.setTitle(std::string(caption, 0, 30));
}

void SFMLRender::dispatch_events()
{
	// Process events
	sf::Event event;
	while (window.pollEvent(event))
	{
		if (event.type == sf::Event::KeyPressed)
		{
			switch( event.key.code )
			{
				case sf::Keyboard::Escape:
					render_call_event_callback(EV_QUIT);
					break;
				case sf::Keyboard::Up:
					render_call_event_callback(EV_KEY_UP);
					break;

				case sf::Keyboard::Down:
					render_call_event_callback(EV_KEY_DOWN);
					break;

				case sf::Keyboard::Right:
					render_call_event_callback(EV_KEY_RIGHT);
					break;

				case sf::Keyboard::Left:
					render_call_event_callback(EV_KEY_LEFT);
					break;

				case sf::Keyboard::Space:
					render_call_event_callback(EV_KEY_SPACE);
					break;

				case sf::Keyboard::I:
					render_call_event_callback(EV_KEY_I);
					break;

				case sf::Keyboard::V:
					render_call_event_callback(EV_KEY_V);
					break;

				default:
					break;
			}
		}

		// Close window: exit
		if (event.type == sf::Event::Closed)
		{
			if(verbosity > 0)
				std::cout<< "RENDER:(SFML) (event) close\n"<< std::endl;
			render_call_event_callback(EV_QUIT);
		}
	}
}


/******************************* C wrapper functions ********************************/
SFMLRender* render = NULL;

/*
 * init sfml render
 * args:
 *    width - overlay width
 *    height - overlay height
 *    flags - window flags:
 *              0- none
 *              1- fullscreen
 *              2- maximized
 *
 * asserts:
 *
 * returns: error code (0 ok)
 */
 int init_render_sfml(int width, int height, int flags)
 {
	//clean old render
	if(render)
		delete(render);
		
	render = new SFMLRender(width, height, flags);

	if(!render)
	{
		std::cerr << "RENDER: (SFML) couldn't create render" << std::endl;
		return -1;
	}
	
	if(!render->has_window())
	{
		std::cerr << "RENDER: (SFML) couldn't create render window" << std::endl;
		return -2;
	}
	
	return 0;
 }

/*
 * render a frame
 * args:
 *   frame - pointer to frame data (yuyv format)
 *   width - frame width
 *   height - frame height
 *
 * asserts:
 *   poverlay is not nul
 *   frame is not null
 *
 * returns: error code
 */
int render_sfml_frame(uint8_t *frame, int width, int height)
{
	return render->render_frame(frame, width, height);
}

/*
 * set sfml render caption
 * args:
 *   caption - string with render window caption
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_render_sfml_caption(const char* caption)
{
	render->set_caption(caption);
}

/*
 * dispatch sfml render events
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void render_sfml_dispatch_events()
{
	render->dispatch_events();
}

/*
 * clean sfml render data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void render_sfml_clean()
{
	delete(render);
	render = NULL;
}

