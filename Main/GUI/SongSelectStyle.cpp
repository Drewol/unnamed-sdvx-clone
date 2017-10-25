#include "stdafx.h"
#include "SongSelectStyle.hpp"
#include "Shared/Jobs.hpp"
#include "Application.hpp"

Ref<SongSelectStyle> SongSelectStyle::instance;
Ref<SongSelectStyle> SongSelectStyle::Get(Application* application)
{
	if(!instance)
	{
		assert(application);
		instance = Ref<SongSelectStyle>(new SongSelectStyle(application));
	}
	return instance;
}

SongSelectStyle::SongSelectStyle(Application* application)
{
	frameSub = application->LoadTexture("song_select/sub.png");
	frameSub->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	frameMain = application->LoadTexture("song_select/main.png");
	frameMain->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	loadingJacketImage = application->LoadTexture("song_select/loading.png");
	static const char* diffTextures[] =
	{
		"song_select/nov.png",
		"song_select/adv.png",
		"song_select/exh.png",
		"song_select/grv.png",
		"song_select/inf.png",
	};
	for(uint32 i = 0; i < 5; i++)
	{
		diffFrames[i] = application->LoadTexture(diffTextures[i]);
		diffFrames[i]->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	}
	diffFrameMaterial = application->LoadMaterial("diffFrame");
	diffFrameMaterial->opaque = false;
}
SongSelectStyle::~SongSelectStyle()
{
	for(auto t : m_jacketImages)
	{
		if (t.second){
			t.second->loadingJob->Terminate();
			delete t.second;
		}
	}
}

Texture SongSelectStyle::GetJacketThumnail(const String& path)
{
	Texture ret = loadingJacketImage;

	auto it = m_jacketImages.find(path);
	if(it == m_jacketImages.end() || !it->second)
	{
		CachedJacketImage* newImage = new CachedJacketImage();
		JacketLoadingJob* job = new JacketLoadingJob();
		job->imagePath = path;
		job->target = newImage;
		newImage->loadingJob = Ref<JobBase>(job);
		newImage->lastUsage = m_timer.SecondsAsFloat();
		g_jobSheduler->Queue(newImage->loadingJob);

		m_jacketImages.Add(path, newImage);
	}
	else
	{
		it->second->lastUsage = m_timer.SecondsAsFloat();
		// If loaded set texture
		if(it->second->texture)
		{
			ret = it->second->texture;
		}
	}

	// cleanup
	/*
	auto now = m_timer.SecondsAsFloat();
	for (auto& jacket : m_jacketImages){
		if (jacket.second){
			// evict jackets not used in past minute
			if (now - jacket.second->lastUsage > 60){
				delete jacket.second;
				jacket.second = nullptr;
			}
		}
	}
	//*/

	return ret;
}
bool JacketLoadingJob::Run()
{
	// Create loading task
	loadedImage = ImageRes::Create(imagePath);
	if (loadedImage.IsValid()){
		if (loadedImage->GetSize().x > 150 || loadedImage->GetSize().y > 150){
			loadedImage->ReSize({150,150});
		}
	}
	return loadedImage.IsValid();
}
void JacketLoadingJob::Finalize()
{
	if(IsSuccessfull())
	{
		target->texture = TextureRes::Create(g_gl, loadedImage);
		target->texture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	}
}