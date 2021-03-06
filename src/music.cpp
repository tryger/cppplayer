#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>

#include "music.h"


//Public functions

void Music::PlayList() {
	while(GetStatus() != Status::Exit) {
		//Wait here until state is not Stoped
		std::mutex cv_m;
		std::unique_lock<std::mutex> lk{cv_m};
		cv.wait(lk, [this]{return IsNotStatus(Status::Stoped);});

		auto& musicList = list.GetSongList();
		for(auto s = musicList.begin(); s != musicList.end(); ++s) {
			if(IsStatus(Status::Exit) || IsStatus(Status::Stoped))		break;
			if(IsStatus(Status::Forwarding))	SetStatus(Status::Playing);

			if(IsStatus(Status::Playing)) {
				song = **s;
				Play();
			}	
			
			if(IsStatus(Status::Backing)) {
				if(s-1 > musicList.begin()) {
					s -= 2;
				}
				SetStatus(Status::Playing);
			}
		}
	}
}

void Music::Play() {
	if(song.GetExtension() == ".mp3") Reproduce(mp3music, song.GetFile().c_str());
	else Reproduce(music, song.GetFile().c_str());
}

Status Music::GetStatus() const {
	return status;
}

void Music::SetStatus(Status s) {
	//Wait previous status to be processed
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk{cv_m};
	status_cv.wait(lk, [this]{return status_processed.load();});

	//Fix bug: when setting two times the same status it stops reading
	if(status == s) return;

	status_processed = false;

	status = s;

	cv.notify_one();
}

void Music::SetVolume(float v) {
	music.setVolume(v);
	mp3music.setVolume(v);
}

float Music::GetVolume() {
	//XXX: mp3music and music have the same value, doesn't
	//matter if we return mp3music.getVolume() or music.getVolume()
	return music.getVolume();
}

int Music::GetRemainingMilliseconds() {
	return (music.getDuration().asMilliseconds() - music.getPlayingOffset().asMilliseconds())
			+ (mp3music.getDuration().asMilliseconds() - mp3music.getPlayingOffset().asMilliseconds());
}

void Music::SetPlayingOffset(int ms) {
	// ms: Millisecond in the song to move the current offset

	auto duration = music.getDuration().asMilliseconds() + mp3music.getDuration().asMilliseconds();

	if(ms > duration)
		ms = duration;

	music.setPlayingOffset(sf::milliseconds(ms));
	mp3music.setPlayingOffset(sf::milliseconds(ms));

	// This is to update the sleep_time
	SetStatus(Status::Paused);
	SetStatus(Status::Playing);
}

bool Music::IsStatus(Status s) {
	bool tmp = GetStatus()==s;
	if(tmp) {
		status_processed = true;
		status_cv.notify_one();
	}
	return tmp;
}

bool Music::IsNotStatus(Status s) {
	bool tmp = GetStatus()!=s;
	if(tmp) {
		status_processed = true;
		status_cv.notify_one();
	}
	return tmp;
}

MusicList& Music::GetList() {
	return list;
}

Song& Music::GetCurrent() {
	return song;
}

//Private functions


//TODO: SFML is not appropiate for a music player
//in a future we should use another library like ffmpeg or OpenAL
template <typename T>
void Music::Reproduce(T& music, const char* song) {
	if(!music.openFromFile(song)) {
		std::cerr << "Can not open file " << song << std::endl;
		return;
	}
	
	
	#ifdef DEBUG
	std::cout << "Playing: " << song << std::endl;
	#endif
	
	auto duration = music.getDuration().asMilliseconds();

	music.play();

	std::mutex cv_m;
	std::unique_lock<std::mutex> lk{cv_m};

	bool loop = true; 
	//loop only if the command is Pause
	while(loop) {
		loop = false;

		//Calculate how many milliseconds we have to sleep for finish the song
		auto offset = music.getPlayingOffset().asMilliseconds();
		auto sleep_time = duration - offset;

		if(sleep_time < 0) {
			//It should never happen
			throw std::logic_error("Playing offset is greater than total length");
		}

		#ifdef DEBUG
		std::cout << "Sleeping " << sleep_time << " milliseconds" << std::endl;
		#endif

		//Wait until we have something to do or until the song finish
		cv.wait_for(lk, std::chrono::milliseconds(sleep_time), [this]{return IsNotStatus(Status::Playing);});

		if(IsStatus(Status::Paused)) {
			music.pause();
			cv.wait(lk, [this]{return IsNotStatus(Status::Paused);});
			music.play();
			loop = true;
		} else if(IsStatus(Status::Restart)) {
			music.setPlayingOffset(sf::seconds(0));
			status = Status::Playing;
		}
	}
	music.stop();
}