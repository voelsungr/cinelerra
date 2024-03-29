
/*
 * CINELERRA
 * Copyright (C) 2009 Adam Williams <broadcast at earthling dot net>
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

#include "file.inc"

#ifdef USE_FILEFORK

#include "bcresources.h"
#include "bcsignals.h"
#include "filefork.h"
#include "fileserver.h"
#include "mutex.h"


#include <unistd.h>


FileServer::FileServer(Preferences *preferences) : ForkWrapper()
{
	this->preferences = preferences;
	lock = new Mutex("FileServer::lock");
}

FileServer::~FileServer()
{
	stop();
	delete lock;
}

void FileServer::init_child()
{
	BC_WindowBase::get_resources()->vframe_shm = 1;
//printf("FileServer::init_child %d %d\n", __LINE__, getpid());
}

int FileServer::handle_command()
{
	const int debug = 0;
	switch(command_token)
	{
		case NEW_FILEFORK:
		{
			FileFork *file_fork = new FileFork(this);
			file_fork->start();
			unsigned char buffer[sizeof(FileFork*) + sizeof(int)];
			FileFork **ffbfr = (FileFork **)buffer;
			*ffbfr = file_fork;
			int *ibfr = (int *)&ffbfr[1];
			*ibfr = file_fork->pid;

			if(debug) printf("FileServer::handle_command NEW_FILEFORK %d parent_fd=%d file_fork=%p\n",
				__LINE__,
				file_fork->parent_fd,
				file_fork);
			send_fd(file_fork->parent_fd);
			send_result(0, buffer, sizeof(FileFork*) + sizeof(int));
			break;
		}

		case DELETE_FILEFORK:
		{
			FileFork **ffbfr = (FileFork **)command_data;
			FileFork *file_fork = *ffbfr;
			if(debug) printf("FileServer::handle_command DELETE_FILEFORK %d file_fork=%p\n",
				__LINE__,
				file_fork);
			delete file_fork;
			break;
		}
	}

	return 0;
}

FileFork* FileServer::new_filefork()
{
	lock->lock("FileServer::open_file");
	FileFork *dummy_fork = new FileFork(this);
// Create real file fork on the server
	send_command(FileServer::NEW_FILEFORK, 0, 0);

	int parent_fd = get_fd();
	read_result();

// Transfer fd to dummy file fork
	dummy_fork->start_dummy(parent_fd, *(int*)(result_data + sizeof(FileFork*)));
	dummy_fork->real_fork = *(FileFork**)result_data;
// printf("FileServer::new_filefork %d parent_fd=%d real_fork=%p\n",
// __LINE__,
// parent_fd,
// dummy_fork->real_fork);
	lock->unlock();
	return dummy_fork;
}

void FileServer::delete_filefork(FileFork *file_fork)
{
	lock->lock("FileServer::close_file");
// Delete filefork on server
	unsigned char buffer[sizeof(FileFork*)];
	FileFork **ffbfr = (FileFork **)buffer;
	*ffbfr = file_fork;
	send_command(FileServer::DELETE_FILEFORK, buffer, sizeof(FileFork*));
	lock->unlock();
}

#endif // USE_FILEFORK





