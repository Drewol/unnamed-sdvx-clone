#include "stdafx.h"
#include <Audio/Audio.hpp>
#include "TestMusicPlayer.hpp"
#include <thread>

using namespace std::this_thread;
using namespace std::chrono;

TestMusicPlayer::TestMusicPlayer()
{

}
void TestMusicPlayer::Init(const String& songPath, uint32 startOffset)
{
	audio = new Audio();
	TestEnsure(audio->Init(false));

	song = audio->CreateStream(songPath);
	TestEnsure(song);

	song->Play();
	song->SetPosition(startOffset);
}
void TestMusicPlayer::Run()
{
	Timer t;
	while (!song->HasEnded())
	{
		float dt = t.SecondsAsFloat();
		t.Restart();
		Update(dt);
		sleep_for(milliseconds(5));
	}
}
void TestMusicPlayer::Update(float dt)
{

}
