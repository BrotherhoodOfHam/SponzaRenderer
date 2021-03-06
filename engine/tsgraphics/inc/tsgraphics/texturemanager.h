/*
	Texture management header
*/

#pragma once

#include <tsgraphics/abi.h>
#include <tsgraphics/api/renderapi.h>
#include <tsgraphics/api/rendercommon.h>

#include <tscore/filesystem/path.h>

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace ts
{
	class CRenderModule;
	class CTextureManager;

	class CTextureCube;

	//Texture class encapsulates the a texture resource and a texture view which can be bound to the pipeline
	class CTexture2D
	{
	private:

		CTextureManager* m_manager = nullptr;

		ResourceProxy m_texRsc;
		ResourceProxy m_texView;

		//Properties
		uint32 m_width = 0;
		uint32 m_height = 0;

		ETextureFormat m_texformat;

	public:

		CTexture2D() {}
		TSGRAPHICS_API CTexture2D(
			CTextureManager* manager,
			const STextureResourceData& data,
			const STextureResourceDescriptor& desc
		);

		ResourceProxy getView() const { return m_texView; }

		uint32 getWidth() const { return m_width; }
		uint32 getHeight() const { return m_height; }
		ETextureFormat getFormat() const { return m_texformat; }
	};
	
	//Texture cube class
	class CTextureCube
	{
	private:

		CTextureManager* m_manager = nullptr;

		ResourceProxy m_texCubeRsc;
		ResourceProxy m_texCubeView;
		ResourceProxy m_texCubeFaceViews[6];

		//Properties
		uint32 m_facewidth = 0;
		uint32 m_faceheight = 0;

		ETextureFormat m_texformat;

	public:

		CTextureCube() {}
		TSGRAPHICS_API CTextureCube(
			CTextureManager* manager,
			const STextureResourceData* data,
			const STextureResourceDescriptor& desc
		);

		ResourceProxy getView() const { return m_texCubeView; }
		ResourceProxy getFaceView(uint32 idx) const { return m_texCubeFaceViews[idx]; }

		uint32 getWidth() const { return m_facewidth; }
		uint32 getHeight() const { return m_faceheight; }
		ETextureFormat getFormat() const { return m_texformat; }
	};

	//Texture manager class which is responsible for controlling the lifetime of textures and loading them from disk
	class CTextureManager
	{
	private:

		CRenderModule* m_renderModule = nullptr;
		Path m_rootpath;
		
		uintptr_t m_token = 0;

	public:

		TSGRAPHICS_API CTextureManager(CRenderModule* module, const Path& rootpath = "");
		TSGRAPHICS_API ~CTextureManager();

		CRenderModule* const getModule() const { return m_renderModule; }

		void setRootpath(const Path& rootpath) { m_rootpath = rootpath; }
		Path getRootpath() const { return m_rootpath; }

		bool TSGRAPHICS_API loadTexture2D(const Path& file, CTexture2D& texture);
		bool TSGRAPHICS_API loadTextureCube(const Path& file, CTextureCube& texture);
	};
}

/////////////////////////////////////////////////////////////////////////////////////////////////