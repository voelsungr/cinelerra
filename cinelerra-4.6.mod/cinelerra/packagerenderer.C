
/*
 * CINELERRA
 * Copyright (C) 2008 Adam Williams <broadcast at earthling dot net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "arender.h"
#include "asset.h"
#include "auto.h"
#include "bctimer.h"
#include "brender.h"
#include "cache.h"
#include "clip.h"
#include "cwindow.h"
#include "cwindowgui.h"
#include "edit.h"
#include "edits.h"
#include "edl.h"
#include "edlsession.h"
#include "errorbox.h"
#include "file.h"
#include "filesystem.h"
#include "format.inc"
#include "indexfile.h"
#include "language.h"
#include "mwindow.h"
#include "mwindowgui.h"
#include "packagerenderer.h"
#include "playabletracks.h"
#include "playbackconfig.h"
#include "pluginserver.h"
#include "preferences.h"
#include "render.h"
#include "renderengine.h"
#include "renderfarmfsserver.h"
#include "samples.h"
#include "sighandler.h"
#include "tracks.h"
#include "transportque.h"
#include "vedit.h"
#include "vframe.h"
#include "videodevice.h"
#include "vrender.h"







RenderPackage::RenderPackage()
{
	audio_start = 0;
	audio_end = 0;
	video_start = 0;
	video_end = 0;
	audio_do = 0;
	video_do = 0;
	path[0] = 0;
	done = 0;
	use_brender = 0;
}

RenderPackage::~RenderPackage()
{
}








// Used by RenderFarm and in the future, Render, to do packages.
PackageRenderer::PackageRenderer()
{
	command = 0;
	audio_cache = 0;
	video_cache = 0;
	aconfig = 0;
	vconfig = 0;
	timer = new Timer;
	frames_per_second = 0;
}

PackageRenderer::~PackageRenderer()
{
	delete command;
	delete audio_cache;
	delete video_cache;
	delete vconfig;
	delete aconfig;
	delete timer;
}


int PackageRenderer::initialize(MWindow *mwindow,
		EDL *edl,
		Preferences *preferences,
		Asset *default_asset)
{
	int result = 0;

	this->mwindow = mwindow;
	this->edl = edl;
	this->preferences = preferences;
	this->default_asset = default_asset;


//printf("PackageRenderer::initialize %d\n", preferences->processors);
	command = new TransportCommand;
	command->command = NORMAL_FWD;
	command->get_edl()->copy_all(edl);
	command->change_type = CHANGE_ALL;
	command->set_playback_range(edl);

	default_asset->frame_rate = command->get_edl()->session->frame_rate;
	default_asset->sample_rate = command->get_edl()->session->sample_rate;
	default_asset->aspect_ratio = (double)command->get_edl()->session->aspect_w /
		command->get_edl()->session->aspect_h;
	result = Render::check_asset(edl, *default_asset);

	audio_cache = new CICache(preferences);
	video_cache = new CICache(preferences);

	//PlaybackConfig *config = command->get_edl()->session->playback_config;
	aconfig = new AudioOutConfig();
	vconfig = new VideoOutConfig;

	return result;
}

void PackageRenderer::create_output()
{
	FileSystem fs;
	asset = new Asset(*default_asset);

	if(!get_master() && preferences->renderfarm_vfs && preferences->use_renderfarm)
		sprintf(asset->path, RENDERFARM_FS_PREFIX "%s", package->path);
	else
		strcpy(asset->path, package->path);




	file = new File;

	file->set_processors(preferences->processors);

//printf("PackageRenderer::create_output %d\n", __LINE__);
	result = file->open_file(preferences,
					asset,
					0,
					1);
//printf("PackageRenderer::create_output %d %d\n", __LINE__, result);

	if(result && mwindow)
	{
// open failed
		char string[BCTEXTLEN];
		sprintf(string, _("Couldn't open %s"), asset->path);
		ErrorBox error(PROGRAM_NAME ": Error",
			mwindow->gui->get_abs_cursor_x(1),
			mwindow->gui->get_abs_cursor_y(1));
		error.create_objects(string);
		error.run_window();
	}
	else
	if(mwindow)
	{
		mwindow->sighandler->push_file(file);
		IndexFile::delete_index(preferences, asset);
	}
//printf("PackageRenderer::create_output %d %d\n", __LINE__, result);
}

void PackageRenderer::create_engine()
{
// Fix audio buffers to 1 second
	audio_read_length = command->get_edl()->session->sample_rate;
	command->get_edl()->session->playback_config->aconfig->fragment_size = audio_read_length;

	aconfig->fragment_size = audio_read_length;


	render_engine = new RenderEngine(0,
		preferences,
		0,
		0,
		0);
	render_engine->set_acache(audio_cache);
	render_engine->set_vcache(video_cache);
	render_engine->arm_command(command);

	if(package->use_brender)
	{
		audio_preroll = Units::to_int64((double)preferences->brender_preroll /
			default_asset->frame_rate *
			default_asset->sample_rate);
		video_preroll = preferences->brender_preroll;
	}
	else
	{
		audio_preroll = Units::to_int64(preferences->render_preroll *
			default_asset->sample_rate);
		video_preroll = Units::to_int64(preferences->render_preroll *
			default_asset->frame_rate);
	}
	audio_position = package->audio_start - audio_preroll;
        if( audio_position < 0 )
	{
		audio_preroll += audio_position;
		audio_position = 0;
	}
	video_position = package->video_start - video_preroll;
	if( video_position < 0 )
	{
		video_preroll += video_position;
		video_position = 0;
	}


//	PRINT_TRACE

// Create output buffers
	if(asset->audio_data)
	{
		file->start_audio_thread(audio_read_length,
			preferences->processors > 1 ? 2 : 1);
	}

//	PRINT_TRACE

	if(asset->video_data)
	{
		compressed_output = new VFrame;
// The write length needs to correlate with the processor count because
// it is passed to the file handler which usually processes frames simultaneously.
		video_write_length = preferences->processors;
		video_write_position = 0;
		direct_frame_copying = 0;


//printf("PackageRenderer::create_engine %d\n", __LINE__);
		file->start_video_thread(video_write_length,
			command->get_edl()->session->color_model,
			preferences->processors > 1 ? 2 : 1,
			0);
//printf("PackageRenderer::create_engine %d\n", __LINE__);


		if(mwindow)
		{
			video_device = new VideoDevice;
 			video_device->open_output(vconfig,
 				command->get_edl()->session->frame_rate,
				command->get_edl()->session->output_w,
				command->get_edl()->session->output_h,
 				mwindow->cwindow->gui->canvas,
				0);
//			video_device->start_playback();
		}
	}


	playable_tracks = new PlayableTracks(render_engine->get_edl(),
		video_position,
		PLAY_FORWARD,
		TRACK_VIDEO,
		1);

}




void PackageRenderer::do_audio()
{
//printf("PackageRenderer::do_audio %d\n", __LINE__);
// Do audio data
	if(asset->audio_data)
	{
		audio_output = file->get_audio_buffer();
// Zero unused channels in output vector
		for(int i = 0; i < MAX_CHANNELS; i++)
		{
			audio_output_ptr[i] = (i < asset->channels) ?
				audio_output[i] :
				0;
		}



// Call render engine

		result = render_engine->arender->process_buffer(audio_output_ptr,
			audio_read_length,
			audio_position);



// Fix buffers for preroll
		int64_t output_length = audio_read_length;
		if(audio_preroll > 0)
		{
			if(audio_preroll >= output_length)
				output_length = 0;
			else
			{
				output_length -= audio_preroll;
				for(int i = 0; i < MAX_CHANNELS; i++)
				{
					if(audio_output_ptr[i])
					{
						double *data = audio_output_ptr[i]->get_data();
						for(int j = 0; j < output_length; j++)
						{
							data[j] = data[j + audio_read_length - output_length];
						}
					}
				}
			}
//printf("PackageRenderer::do_audio 4\n");

			audio_preroll -= audio_read_length;
		}

// Must perform writes even if 0 length so get_audio_buffer doesn't block
		result |= file->write_audio_buffer(output_length);
	}

	audio_position += audio_read_length;
//printf("PackageRenderer::do_audio %d\n", __LINE__);
}


void PackageRenderer::do_video()
{
	const int debug = 0;
// Do video data
	if(asset->video_data)
	{
// get the absolute video position from the audio position
		int64_t video_end = video_position + video_read_length;

		if(video_end > package->video_end)
			video_end = package->video_end;

		while(video_position < video_end && !result)
		{
// Try to copy the compressed frame directly from the input to output files
//printf("PackageRenderer::do_video 2 video_position=%ld\n", video_position);
			if(direct_frame_copy(command->get_edl(),
				video_position,
				file,
				result))
			{
// Direct frame copy failed.
// Switch back to background compression
				if(direct_frame_copying)
				{

					file->start_video_thread(video_write_length,
						command->get_edl()->session->color_model,
						preferences->processors > 1 ? 2 : 1,
						0);
//printf("PackageRenderer::do_video %d %d\n", __LINE__, preferences->processors);
					direct_frame_copying = 0;
				}

// Try to use the rendering engine to write the frame.
// Get a buffer for background writing.

				if(video_write_position == 0)
					video_output = file->get_video_buffer();

				if(debug) printf("PackageRenderer::do_video %d %p\n", __LINE__, video_output);
				if(debug) printf("PackageRenderer::do_video %d %p\n", __LINE__, video_output[0]);




// Construct layered output buffer
				video_output_ptr = video_output[0][video_write_position];
				if(debug)
				{
					printf("PackageRenderer::do_video %d %p\n", __LINE__, video_output_ptr);
					printf("PackageRenderer::do_video %d %d\n", __LINE__, result);
					video_output_ptr->dump();
				}

 				if(!result)
					result = render_engine->vrender->process_buffer(
						video_output_ptr,
						video_position,
						0);


				if(debug) printf("PackageRenderer::do_video %d %d\n", __LINE__, result);

 				if(!result &&
					mwindow &&
					video_device->output_visible())
				{
// Vector for video device
					VFrame *preview_output;

					video_device->new_output_buffer(&preview_output,
						command->get_edl()->session->color_model);

					preview_output->copy_from(video_output_ptr);
					video_device->write_buffer(preview_output,
						command->get_edl());
				}

				if(debug) printf("PackageRenderer::do_video %d %d\n", __LINE__, result);


// Don't write to file
				if(video_preroll && !result)
				{
					video_preroll--;
// Keep the write position at 0 until ready to write real frames
					result = file->write_video_buffer(0);
					video_write_position = 0;
				}
				else
				if(!result)
	 			{
// Set background rendering parameters
// Allow us to skip sections of the output file by setting the frame number.
// Used by background render and render farm.
//printf("PackageRenderer::do_video %d " _LD "\n", __LINE__, video_position);
					video_output_ptr->set_number(video_position);
					video_write_position++;

					if(video_write_position >= video_write_length)
					{
						result = file->write_video_buffer(video_write_position);
//printf("PackageRenderer::do_video %d " _LD "\n", __LINE__, video_position);
// Update the brender map after writing the files.
						if(package->use_brender)
						{
//printf("PackageRenderer::do_video 10\n");
							for(int i = 0; i < video_write_position && !result; i++)
							{
								result = set_video_map(video_position + 1 - video_write_position + i,
									BRender::RENDERED);
							}
//printf("PackageRenderer::do_video 11 %d\n", result);
						}
						video_write_position = 0;
					}
				}
//printf("PackageRenderer::do_video %d " _LD "\n", __LINE__, video_position);


			}

			video_position++;
			if(!result && get_result()) result = 1;
			if(!result && progress_cancelled()) result = 1;
		}
	}
	else
	{
		video_position += video_read_length;
	}
}


void PackageRenderer::stop_engine()
{
	delete render_engine;
	delete playable_tracks;
}


void PackageRenderer::stop_output()
{
	int error = 0;
	if(asset->audio_data)
	{
// stop file I/O
		file->stop_audio_thread();
	}

	if(asset->video_data)
	{
		delete compressed_output;
		if(video_write_position)
			file->write_video_buffer(video_write_position);
		if(package->use_brender)
		{
			for(int i = 0; i < video_write_position && !error; i++)
			{
				error = set_video_map(video_position - video_write_position + i,
					BRender::RENDERED);
			}
		}
		video_write_position = 0;
		if(!error) file->stop_video_thread();
		if(mwindow)
		{
//			video_device->stop_playback();
			video_device->close_all();
			delete video_device;
		}
	}
}


void PackageRenderer::close_output()
{
	if(mwindow)
		mwindow->sighandler->pull_file(file);
	file->close_file();
	delete file;
	asset->Garbage::remove_user();
}

// Aborts and returns 1 if an error is encountered.
int PackageRenderer::render_package(RenderPackage *package)
{
	int audio_done = 0;
	int video_done = 0;
	int samples_rendered = 0;
	const int debug = 0;


	result = 0;
	this->package = package;

// printf(
// "PackageRenderer::render_package: audio s=" _LD " l=" _LD " video s=" _LD " l=" _LD "\n",
// 	package->audio_start,
// 	package->audio_end - package->audio_start,
// 	package->video_start,
// 	package->video_end - package->video_start);

	if(debug) PRINT_TRACE

	if( package->video_do ) default_asset->video_data = 1;
	if( package->audio_do ) default_asset->audio_data = 1;
	Render::check_asset(edl, *default_asset);

	create_output();
	if(debug) PRINT_TRACE

	if(!asset->video_data) video_done = 1;
	if(!asset->audio_data) audio_done = 1;

// Create render engine
	if(!result)
	{
if(debug) PRINT_TRACE
		create_engine();
if(debug) PRINT_TRACE


// Main loop
		timer->update();
		total_samples_rendered = 0;
		while((!audio_done || !video_done) && !result)
		{
			int need_audio = 0, need_video = 0;




// Calculate lengths to process.  Audio fragment is constant.
			if(!audio_done)
			{
				if(audio_position + audio_read_length >= package->audio_end)
				{
					audio_done = 1;
					audio_read_length = package->audio_end - audio_position;
				}

				samples_rendered = audio_read_length;
				need_audio = 1;
			}

//printf("PackageRenderer::render_package 6 %d\n", samples_rendered);

			if(!video_done)
			{
				if(audio_done)
				{
// 					video_read_length = package->video_end - video_position;
// // Packetize video length so progress gets updated
// 					video_read_length = (int)MIN(asset->frame_rate, video_read_length);
// 					video_read_length = MAX(video_read_length, 30);
					video_read_length = 1;
				}
				else
// Guide video with audio
				{
					video_read_length = Units::to_int64(
						(double)(audio_position + audio_read_length) /
						asset->sample_rate *
						asset->frame_rate) -
						video_position;
				}

// Clamp length
				if(video_position + video_read_length >= package->video_end)
				{
					video_done = 1;
					video_read_length = package->video_end - video_position;
				}

// Calculate samples rendered for progress bar.
				if(audio_done)
					samples_rendered = Units::round((double)video_read_length /
						asset->frame_rate *
						asset->sample_rate);

				need_video = 1;
			}
			if(debug) PRINT_TRACE

			if(debug) printf("PackageRenderer::render_package 1 %d " _LD " " _LD "\n",
					result, audio_read_length, video_read_length);
			if(need_video && !result) do_video();
			if(debug) printf("PackageRenderer::render_package %d %d %d\n",
					__LINE__, result, samples_rendered);
			if(need_audio && !result) do_audio();


			if(debug) PRINT_TRACE
			if(!result)
			{
// Calculate frames per second for the renderfarm table.
				total_samples_rendered += samples_rendered;
				if(!video_done && timer->get_difference() > 30000)
				{
					frames_per_second = (double)total_samples_rendered *
						asset->frame_rate /
						asset->sample_rate /
						((double)timer->get_difference() / 1000);
				}
				set_progress(samples_rendered);
			}
			if(debug) PRINT_TRACE





			if(!result && progress_cancelled()) result = 1;
			if(debug) PRINT_TRACE

// printf("PackageRenderer::render_package 10 %d %d %d %d\n",
// audio_read_length, video_read_length, samples_rendered, result);
			if(result)
				set_result(result);
			else
				result = get_result();
		}

// Final FPS readout
		frames_per_second = (double)(package->video_end - package->video_start) /
			((double)timer->get_difference() / 1000);


//PRINT_TRACE
		stop_engine();
//PRINT_TRACE

		stop_output();
//PRINT_TRACE


	}


//PRINT_TRACE

	close_output();

//PRINT_TRACE

	set_result(result);
//PRINT_TRACE



	return result;
}








// Try to copy the compressed frame directly from the input to output files
// Return 1 on failure and 0 on success
int PackageRenderer::direct_frame_copy(EDL *edl,
	int64_t &video_position,
	File *file,
	int &error)
{
	Track *playable_track = 0;
	Edit *playable_edit = 0;

//printf("Render::direct_frame_copy 1\n");
	if(direct_copy_possible(edl,
		video_position,
		playable_track,
		playable_edit,
		file))
	{
// Switch to direct copying
		if(!direct_frame_copying)
		{
			if(video_write_position)
			{
				error |= file->write_video_buffer(video_write_position);
				video_write_position = 0;
			}
			file->stop_video_thread();
			direct_frame_copying = 1;
		}
//printf("Render::direct_frame_copy 2\n");

		if(!package->use_brender)
			error |= ((VEdit*)playable_edit)->read_frame(compressed_output,
				video_position,
				PLAY_FORWARD,
				video_cache,
				1,
				0,
				0);


		if(!error && video_preroll > 0)
		{
			video_preroll--;
		}
		else
		if(!error)
		{
			if(package->use_brender)
			{
//printf("PackageRenderer::direct_frame_copy 1\n");
				error = set_video_map(video_position, BRender::SCANNED);
//printf("PackageRenderer::direct_frame_copy 10 %d\n", error);
			}
			else
			{
				VFrame ***temp_output = new VFrame**[1];
				temp_output[0] = new VFrame*[1];
				temp_output[0][0] = compressed_output;
				error = file->write_frames(temp_output, 1);
				delete [] temp_output[0];
				delete [] temp_output;
			}
		}
		return 0;
	}
	else
		return 1;
}

int PackageRenderer::direct_copy_possible(EDL *edl,
				int64_t current_position,
				Track* &playable_track,  // The one track which is playable
				Edit* &playable_edit, // The edit which is playing
				File *file)   // Output file
{
	int result = 1;
	int total_playable_tracks = 0;
	Track* current_track;

// Number of playable tracks must equal 1
	for(current_track = edl->tracks->first;
		current_track && result;
		current_track = current_track->next)
	{
		if(current_track->data_type == TRACK_VIDEO)
		{
			if(playable_tracks->is_playable(current_track,
				current_position,
				PLAY_FORWARD,
				1))
			{
				playable_track = current_track;
				total_playable_tracks++;
			}
		}
	}

//printf("Render::direct_copy_possible 1 %d\n", result);
	if(total_playable_tracks != 1) result = 0;
//printf("Render::direct_copy_possible 2 %d\n", result);

// Edit must have a source file
// TODO: descend into nested EDL's
	if(result)
	{
//printf("Render::direct_copy_possible 3 %d\n", result);
		playable_edit = playable_track->edits->get_playable_edit(current_position, 1);
//printf("Render::direct_copy_possible 4 %d %p\n", result, playable_edit);
		if(!playable_edit)
			result = 0;
	}

// Source file must be able to copy to destination file.
// Source file must be same size as project output.
	if(result)
	{
		if(!file->can_copy_from(playable_edit->asset,
			current_position + playable_track->nudge,
			edl->session->output_w,
			edl->session->output_h))
			result = 0;
	}
//printf("Render::direct_copy_possible 6 %d\n", result);

// Test conditions mutual between vrender.C and this.
	if(result &&
		!playable_track->direct_copy_possible(current_position, PLAY_FORWARD, 1))
		result = 0;
//printf("Render::direct_copy_possible 7 %d\n", result);

	return result;
}









int PackageRenderer::get_master()
{
	return 0;
}

// Get result status from server
int PackageRenderer::get_result()
{
	return 0;
}

void PackageRenderer::set_result(int value)
{
}

void PackageRenderer::set_progress(int64_t value)
{
}

int PackageRenderer::set_video_map(int64_t position, int value)
{
	return 0;
}

int PackageRenderer::progress_cancelled()
{
	return 0;
}










