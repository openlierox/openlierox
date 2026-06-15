/*
 *  OpenExternBrowser.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 29.09.08.
 *  code under LGPL
 *
 */


#include <string>
#include <list>
#include <cstdlib>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
// This confused Boost. Comes from <AssertMacros.h>
#undef check
#undef __Check
#endif

#ifdef WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "Debug.h"
#include "LieroX.h"


void OpenLinkInExternBrowser(const std::string& url) {
	notes << "open in extern browser: " << url << endl;

#if defined(__APPLE__)
	// Thanks to Jooleem project (http://jooleem.sourceforge.net) for the code

	// Create a string ref of the URL:
	CFStringRef cfurlStr = CFStringCreateWithCString( NULL, url.c_str(), kCFStringEncodingASCII);

	// Create a URL object:
	CFURLRef cfurl = CFURLCreateWithString (NULL, cfurlStr, NULL);

	// Open the URL:
	LSOpenCFURLRef(cfurl, NULL);

	// Release the created resources:
	CFRelease(cfurl);
	CFRelease(cfurlStr);

#elif defined(WIN32)
	ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_MAXIMIZE);
	
#elif defined(__ANDROID__)

	std::string cmd = "am start -a android.intent.action.VIEW --user -3 -d "
	cmd = url;
	::system(cmd.c_str());

#else
	std::string browser = "";
		
	// test some browsers and take the first found		
	std::list<std::string> browsers;

	if (getenv("BROWSER") != NULL) {
		std::string tmp = getenv("BROWSER");
		if(tmp != "") browsers.push_back(tmp);
	}
	browsers.push_back("xdg-open"); // part of XdgUtils from FreeDesktop.org
	browsers.push_back("gnome-open"); // available on most Gnome systems
	browsers.push_back("sensible-browser"); // Ubuntu and others seem to provide this
	browsers.push_back("firefox");
	browsers.push_back("mozilla-firefox");
	browsers.push_back("konqueror");
	browsers.push_back("mozilla");
	browsers.push_back("opera");
	browsers.push_back("epiphany");
	browsers.push_back("galeon");
	browsers.push_back("netscape");

	for (std::list<std::string>::const_iterator it = browsers.begin(); it != browsers.end(); ++it) {
		// we have browser != "" here
		if(((*it)[0] == '/' && ::system(("test -x " + *it).c_str()) == 0) ||
			::system(("test -x /usr/bin/" + *it + " -o -x /usr/bin/X11/" + *it + " -o -x /usr/local/bin/" + *it).c_str()) == 0) {
			browser = *it;
			break;
		}
	}
	
	if(browser == "") {
		warnings << "no browser found" << endl;
		return;
	} else
		notes << "Using " << browser << " as your default browser" << endl;
	
	int r = ::system((browser + " " + url + " &").c_str());
	if(r == -1)
		warnings << "error when executing " << browser << endl;
	else if(r > 0)
		warnings << browser << " returned with error" << endl;
#endif
}

