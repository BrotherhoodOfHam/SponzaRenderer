/*
	Engine definition
*/

#include <tsengine.h>
#include <tscore/debug/assert.h>
#include <tscore/debug/log.h>
#include <tscore/system/info.h>
#include <tscore/system/thread.h>
#include <tscore/system/error.h>
#include <tscore/filesystem/pathhelpers.h>
#include <tsgraphics/colour.h>

//Modules
#include <tsgraphics/rendermodule.h>
#include <tsengine/input/inputmodule.h>

#include <tsengine/platform/window.h>
#include "platform/console.h"

#include <tsengine/event/messenger.h>
#include <tsengine/cmdargs.h>
#include <tsengine/configfile.h>

using namespace ts;
using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Application window class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class EngineWindow :
	public CWindow,
	public CWindow::IEventListener
{
private:

	CEngineEnv& m_env;

	//Window event listener
	int onWindowEvent(const SWindowEventArgs& args) override
	{
		switch (args.eventcode)
		{
		case EWindowEvent::eEventResize:
		{
			uint32 w = 1;
			uint32 h = 1;
			getWindowResizeEventArgs(args, w, h);
			if (auto render = m_env.getRenderModule())
			{
				render->setDisplayConfiguration(eDisplayUnknown, w, h, SMultisampling(0));
			}

			break;
		}
		case EWindowEvent::eEventDestroy:
		{
			m_env.getRenderModule()->setDisplayConfiguration(eDisplayWindowed, 0, 0, 0);

			break;
		}
		}

		return 0;
	}

public:

	CEngineEnv& getSystem() { return m_env; }

	EngineWindow(CEngineEnv& env, const SWindowDesc& desc) :
		m_env(env),
		CWindow(desc)
	{
		this->addEventListener((IEventListener*)this);
	}

	~EngineWindow()
	{
		this->removeEventListener((IEventListener*)this);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Engine initialization
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CEngineEnv::CEngineEnv(const SEngineStartupParams& params)
{
	/////////////////////////////////////////////////////////////////////////
	//Initialize debug layer
	/////////////////////////////////////////////////////////////////////////

	ts::internal::initializeSystemExceptionHandlingFilter();	//This sets the global exception handling filter for the process, meaning uncaught exceptions can be detected and a minidump can be generated
#ifdef _DEBUG
	ts::internal::initializeSystemMemoryLeakDetector();			//This allows the crt to detect memory leaks
#endif

	/////////////////////////////////////////////////////////////////////////
	//Parse command line arguments and load config
	/////////////////////////////////////////////////////////////////////////
	CommandLineArgs args(params.argv, params.argc);

	//Console initialization
	if (!args.isArgumentTag("noconsole"))
	{
		consoleOpen();
		//setConsoleClosingHandler(consoleClosingHandlerFunc);
	}

	//Config loader
	Path cfgpath(params.appPath);
	cfgpath.addDirectories((args.isArgumentTag("config")) ? args.getArgumentValue("config") : "config.ini");
	ConfigFile config(cfgpath);

	//Create cvar table
	m_cvarTable.reset(new CVarTable());
	
	if (config.isSection("CVars"))
	{
		ConfigFile::SPropertyArray properties;
		config.getSectionProperties("CVars", properties);
		
		for (auto p : properties)
		{
			m_cvarTable->setVar(p.key, p.value);
		}
	}

	/////////////////////////////////////////////////////////////////////////
	
	//Set application window parameters
	uint32 width = 800;
	uint32 height = 600;
	config.getProperty("video.resolutionW", width);
	config.getProperty("video.resolutionH", height);

	SWindowDesc windesc;
	windesc.title = params.appPath.str();
	windesc.width = width;
	windesc.height = height;
	
	//Create application window object
	m_window.reset(new EngineWindow(*this, windesc));

	//Runs window message loop on separate thread
	m_window->open(params.showWindow);
	
	uint32 displaymode = 0;
	config.getProperty("video.displaymode", displaymode);
	if (displaymode > 2)
	{
		tswarn("% is not a valid fullscreen mode", displaymode);
		displaymode = 0;
	}

	string assetpathbuf;
	config.getProperty("system.assetdir", assetpathbuf);
	Path assetpath = params.appPath;
	if (isAbsolutePath(assetpathbuf))
		assetpath = assetpathbuf;
	else
		assetpath.addDirectories(assetpathbuf);

	uint32 samplecount = 1;
	config.getProperty("video.multisamplecount", samplecount);

	SRenderModuleConfiguration rendercfg;
	rendercfg.windowHandle = m_window->nativeHandle();
	rendercfg.width = width;
	rendercfg.height = height;
	rendercfg.apiEnum = ERenderApiID::eRenderApiD3D11;
	rendercfg.displaymode = (EDisplayMode)(displaymode + 1);
	rendercfg.rootpath = assetpath;
	rendercfg.multisampling.count = samplecount;

	m_renderModule.reset(new CRenderModule(rendercfg));
	m_inputModule.reset(new CInputModule(m_window.get()));

	/////////////////////////////////////////////////////////////////////////
}

CEngineEnv::~CEngineEnv()
{
	//Shutdown
	m_inputModule.reset();
	m_renderModule.reset();
	m_window.reset();

	consoleClose();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CEngineEnv::start(IApplication& app)
{
	if (int err = app.onInit())
	{
		tserror("Failed to load application class (%)", err);
		return err;
	}

	/////////////////////////////////////////////////////////////////////////
	{
		Timer timer;
		Stopwatch watch;
		double dt = 0.0;

		//Main engine loop
		while (m_window->handleEvents())
		{
			watch.start();

			//timer.tick();

			Vector framecolour = colours::AntiqueWhite;

			//Clear the frame to a specific colour
			m_renderModule->drawBegin(framecolour);

			//Update application
			app.onUpdate(dt);

			//Present the frame
			m_renderModule->drawEnd();

			watch.stop();
			dt = max(0.0, watch.deltaTime()); //clamp delta time to positive value - just to be safe
		}
	}

	app.onExit();

	return 0;
}

void CEngineEnv::shutdown()
{
	m_window->close();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////