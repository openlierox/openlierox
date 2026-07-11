/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// HTTP class implementation
// Created 9/4/02
// Jason Boettcher


#include <cassert>
#include <atomic>
#ifdef WIN32
	#include <windows.h>
	#include <wininet.h>
#else
	#include <stdlib.h>
#endif
#include <curl/curl.h>
#include <curl/easy.h>

#include "LieroX.h"
#include "Debug.h"
#include "FindFile.h"
#include "Options.h"
#include "HTTP.h"
#include "Timer.h"
#include "StringUtils.h"
#include "olx-types.h"
#include "Version.h"
#include "MathLib.h"
#include "InputEvents.h"
#include "ReadWriteLock.h"

// Some basic defines
#define		HTTP_TIMEOUT	10	// Filebase became laggy lately, so increased that from 5 seconds
//#define		BUFFER_LEN		8192

// Abort a transfer that stays below this speed (bytes/sec) for this long (sec).
// Unlike CURLOPT_TIMEOUT this never aborts a healthy but slow large transfer,
// it only fires when the connection has effectively stalled.
#define		HTTP_LOW_SPEED_LIMIT	30
#define		HTTP_LOW_SPEED_TIME		30

// Set on shutdown to make every running transfer return promptly.
static std::atomic<bool> httpTransfersAborting(false);

void SetHttpTransfersAborting(bool aborting) {
	httpTransfersAborting = aborting;
}


//
// Functions
//

/////////////////
// Automatically updates tLXOptions->sHttpProxy with proxy settings retrieved from the system
void AutoSetupHTTPProxy()
{
	// User doesn't wish an automatic proxy setup
	if (!tLXOptions->bAutoSetupHttpProxy) {
		notes << "AutoSetupHTTPProxy is disabled, ";
		if(tLXOptions->sHttpProxy != "")
			notes << "using proxy " << tLXOptions->sHttpProxy << endl;
		else
			notes << "not using any proxy" << endl;
		return;
	}
	
	if(tLXOptions->sHttpProxy != "") {
		notes << "AutoSetupHTTPProxy: we had the proxy " << tLXOptions->sHttpProxy << " but we are trying to autodetect it now" << endl;
	}
	
// DevCpp won't compile this - too old header files
#if defined( WIN32 ) && defined( MSC_VER )
	// Create the list of options we want to retrieve
	INTERNET_PER_CONN_OPTION_LIST List;
	INTERNET_PER_CONN_OPTION Options[2];
	DWORD size = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	// Options we need
	Options[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
	Options[1].dwOption = INTERNET_PER_CONN_FLAGS;

	// Fill the list info
	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = NULL;
	List.dwOptionCount = 2;
	List.dwOptionError = 0;
	List.pOptions = Options;

	// Ask for proxy info
	if(InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &size)) {

		// Using proxy?
		bool using_proxy = (Options[1].Value.dwValue & PROXY_TYPE_PROXY) == PROXY_TYPE_PROXY;
		
		if (using_proxy)  {
			if (Options[0].Value.pszValue != NULL)  { // Safety check
				tLXOptions->sHttpProxy = Options[0].Value.pszValue; // Set the proxy

				// Remove http:// if present
				static const size_t httplen = 7; // length of "http://"
				if( stringcaseequal(tLXOptions->sHttpProxy.substr(0, httplen), "http://") )
					tLXOptions->sHttpProxy.erase(0, httplen);

				// Remove trailing slash
				if (*tLXOptions->sHttpProxy.rbegin() == '/')
					tLXOptions->sHttpProxy.resize(tLXOptions->sHttpProxy.size() - 1);

				notes << "Using HTTP proxy: " << tLXOptions->sHttpProxy << endl;
			}
		} else {
			tLXOptions->sHttpProxy = ""; // No proxy
		}

		// Cleanup
		if(Options[0].Value.pszValue != NULL)
			GlobalFree(Options[0].Value.pszValue);
	}
	
#else
	// Linux has numerous configuration of proxies for each application, but environment var seems to be the most common
	const char * c_proxy = getenv("http_proxy");
	if( c_proxy == NULL )  {
		c_proxy = getenv("HTTP_PROXY");
	}

	if(c_proxy) {
		// Get the value (string after '=' char)
		std::string proxy(c_proxy);
		if( proxy.find('=') == std::string::npos )  { // No proxy
			tLXOptions->sHttpProxy = "";
			return;
		}
		proxy = proxy.substr( proxy.find('=') + 1 );
		TrimSpaces(proxy);
	
		// Remove http:// if present
		static const size_t httplen = 7; // length of "http://"
		if( stringcaseequal(proxy.substr(0, httplen), "http://") )
			proxy.erase(0, httplen);

		// Blank proxy?
		if( proxy != "" )  {
			// Remove trailing slash
			if( *proxy.rbegin() == '/')
				proxy.resize( proxy.size() - 1 );
		}

		tLXOptions->sHttpProxy = proxy;

		notes << "AutoSetupHTTPProxy: " << proxy << endl;
	}	
#endif
}



struct CurlThread : Action {

	CurlThread( CHttp * parent, CURL * _curl ) :
		parent( parent ),
		curl( _curl ),
		curlForm( NULL ),
		aborted( false )
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlReceiveCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		// The progress callback runs about once a second even while the transfer
		// is stalled, so it is the only place we can abort a hung request from.
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, (long) 0);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlProgressCallback);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)this);
	}

	Result handle();

	CHttp *			parent;
	CURL *			curl;
	curl_httppost *	curlForm;
	Mutex			Lock;
	std::atomic<bool> aborted; // set by CancelProcessing to interrupt curl_easy_perform

	static size_t CurlReceiveCallback(void *ptr, size_t size, size_t nmemb, void *data);
	static int CurlProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
};

CHttp::CHttp()
{
	ProcessingResult = HTTP_PROC_FINISHED;
	HTTPStatusCode = 0;
	DownloadStart = DownloadEnd = 0;
	curlThread = NULL;
}

CHttp::~CHttp()
{
	CancelProcessing();
}

size_t CurlThread::CurlReceiveCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	CurlThread* self = (CurlThread *)data;
	size_t realsize = size * nmemb;
	
	Mutex::ScopedLock l(self->Lock);

	if( !self->parent ) // Aborting
		return 0;
	
	Mutex::ScopedLock l1(self->parent->Lock);
	self->parent->Data.append((const char *)ptr, realsize);
	
	return realsize;
}

int	CurlThread::CurlProgressCallback(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
	CurlThread* self = (CurlThread *)clientp;
	// A non-zero return aborts curl_easy_perform with CURLE_ABORTED_BY_CALLBACK.
	// aborted is set when the request was cancelled,
	// httpTransfersAborting on shutdown.
	// Both are atomic, so no lock is needed here.
	if( self->aborted || httpTransfersAborting )
		return 1;
	return 0;
}

CURL * CHttp::InitializeTransfer(const std::string& url, const std::string& proxy)
{
	CancelProcessing();
	ProcessingResult = HTTP_PROC_PROCESSING;
	Url = url;
	Proxy = proxy;
	Useragent = GetFullGameName();
	Data = "";
	HTTPStatusCode = 0;
	DownloadStart = DownloadEnd = tLX->currentTime;

	CURL * curl = curl_easy_init();
	curl_easy_setopt( curl, CURLOPT_URL, Url.c_str() );
	curl_easy_setopt( curl, CURLOPT_PROXY, Proxy.c_str() );
	curl_easy_setopt( curl, CURLOPT_USERAGENT, Useragent.c_str() );
	curl_easy_setopt( curl, CURLOPT_NOSIGNAL, (long) 1 );
	curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, (long) HTTP_TIMEOUT );
	curl_easy_setopt( curl, CURLOPT_LOW_SPEED_LIMIT, (long) HTTP_LOW_SPEED_LIMIT );
	curl_easy_setopt( curl, CURLOPT_LOW_SPEED_TIME, (long) HTTP_LOW_SPEED_TIME );
	curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, (long) 1 ); // Allow server to use 3XX Redirect codes
	curl_easy_setopt( curl, CURLOPT_MAXREDIRS, (long) 25 ); // Some reasonable limit
#ifdef CURLSSLOPT_NATIVE_CA
	// Verify HTTPS certificates against the OS certificate store. Without this,
	// the OpenSSL build of libcurl looks for a CA bundle file that is not
	// shipped with the packaged Windows build, so every https:// request fails.
	curl_easy_setopt( curl, CURLOPT_SSL_OPTIONS, (long) CURLSSLOPT_NATIVE_CA );
#endif
	// If a CA bundle is shipped with the game (e.g. the Android APK ships
	// Mozilla's cacert.pem for libcurl's mbedTLS backend, which has no
	// system cert store to fall back to), point curl at it. Returns "" on
	// platforms where no bundle is shipped, in which case we leave the
	// default verification path in place.
	{
		static const std::string caBundle = GetFullFileName("cacert.pem");
		if(!caBundle.empty())
			curl_easy_setopt( curl, CURLOPT_CAINFO, caBundle.c_str() );
	}
	//curl_easy_setopt( curl, CURLOPT_TIMEOUT, (long) HTTP_TIMEOUT ); // Do not set this if you don't want abort in the middle of large transfer
	return curl;
}

void CHttp::RequestData(const std::string& url, const std::string& proxy)
{
	CURL * curl = InitializeTransfer(url, proxy);
	curlThread = new CurlThread(this, curl);
	threadPool->start(curlThread, "CHttp: " + Url);
}

void CHttp::SendData(const std::list<HTTPPostField>& data, const std::string url, const std::string& proxy)
{
	CURL * curl = InitializeTransfer(url, proxy);
	
	curl_httppost * curlForm = NULL;
	struct curl_httppost *lastptr = NULL;
	
	for( std::list<HTTPPostField> :: const_iterator it = data.begin(); it != data.end(); it++ )
	{
		if( it->getFileName() == "" )
			curl_formadd(	&curlForm,
							&lastptr,
							CURLFORM_COPYNAME, it->getName().c_str(),
							CURLFORM_CONTENTSLENGTH, it->getData().size(),
							CURLFORM_COPYCONTENTS, it->getData().c_str(),
							CURLFORM_END);
		else
			curl_formadd(	&curlForm,
							&lastptr,
							CURLFORM_COPYNAME, it->getName().c_str(),
							CURLFORM_FILENAME, it->getFileName().c_str(),
							CURLFORM_CONTENTTYPE, it->getMimeType().c_str(),
							CURLFORM_CONTENTSLENGTH, it->getData().size(),
							CURLFORM_COPYCONTENTS, it->getData().c_str(),
							CURLFORM_END);
	}
	
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, curlForm);
	curlThread = new CurlThread(this, curl);
	curlThread->curlForm = curlForm;
	threadPool->start(curlThread, "CHttp: " + Url);
}

Result CurlThread::handle()
{
	CURLcode res = curl_easy_perform(curl); // Blocks until processing finished

	Mutex::ScopedLock l(Lock);
	if( parent != NULL )
	{
		Mutex::ScopedLock l1(parent->Lock);

		parent->curlThread = NULL;
		
		parent->ProcessingResult = HTTP_PROC_FINISHED;
		parent->Error.iError = HTTP_NO_ERROR;
		if( res != CURLE_OK )
		{
			parent->ProcessingResult = HTTP_PROC_ERROR;
			parent->Error.iError = res; // This is not HTTP error, it's libcurl error, but whatever
			parent->Error.sErrorMsg = curl_easy_strerror(res);
		}
		parent->DownloadEnd = tLX->currentTime;

		// Remember the HTTP status code so callers can reject error responses.
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &parent->HTTPStatusCode);

		if( curlForm != NULL )
			curl_formfree(curlForm);
		curlForm = NULL;
		
		parent->onFinished.occurred(CHttp::HttpEventData(parent, parent->ProcessingResult == HTTP_PROC_FINISHED));
		parent = NULL;
	}
	
	curl_easy_cleanup(curl);

	return true;
}

void CHttp::CancelProcessing() // Non-blocking
{
	Mutex::ScopedLock l(Lock);
	
	if(curlThread != NULL)
	{
		Mutex::ScopedLock l(curlThread->Lock);
		// Interrupt a still-running transfer via the progress callback,
		// so the worker thread returns instead of blocking in curl_easy_perform.
		curlThread->aborted = true;
		curlThread->parent = NULL;
		curlThread = NULL;
	}
}

size_t CHttp::GetDataLength() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));	
	return Data.size();
}

std::string CHttp::GetMimeType() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));
	if( !curlThread )
		return "";
	Mutex::ScopedLock l1(curlThread->Lock);
	const char * c = NULL;
	curl_easy_getinfo(curlThread->curl, CURLINFO_CONTENT_TYPE, c);
	std::string ret;
	if( c != NULL )
		ret = c;
	return ret;
}

float CHttp::GetDownloadSpeed() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));
	if( !curlThread )
		return 0;
	Mutex::ScopedLock l1(curlThread->Lock);
	double d = 0;
	curl_easy_getinfo(curlThread->curl, CURLINFO_SPEED_DOWNLOAD, &d);
	return float(d);
}

float CHttp::GetUploadSpeed() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));
	double d = 0;
	if( !curlThread )
		return 0;
	Mutex::ScopedLock l1(curlThread->Lock);
	curl_easy_getinfo(curlThread->curl, CURLINFO_SPEED_UPLOAD, &d);
	return float(d);
}

std::string CHttp::GetHostName() const
{
	std::string sHost;
	size_t s = 0;
	if( Url.find("http://") != std::string::npos )
		s = Url.find("http://") + 7;

	size_t p = Url.find("/", s );
	if(p == std::string::npos) 
		sHost = Url.substr(s);
	else
		sHost = Url.substr(s, p-s);

	return sHost;
}

size_t CHttp::GetDataToSendLength() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));
	double d = 0;
	if( !curlThread )
		return 0;
	Mutex::ScopedLock l1(curlThread->Lock);
	curl_easy_getinfo(curlThread->curl, CURLINFO_CONTENT_LENGTH_UPLOAD, &d);
	if( d <= 0 )
		d = 1;
	return (size_t)d;
};

size_t CHttp::GetSentDataLen() const
{
	Mutex::ScopedLock l(const_cast<Mutex &>(Lock));
	double d = 0, total = 0;
	if( !curlThread )
		return 0;
	Mutex::ScopedLock l1(curlThread->Lock);
	curl_easy_getinfo(curlThread->curl, CURLINFO_CONTENT_LENGTH_UPLOAD, &total);
	curl_easy_getinfo(curlThread->curl, CURLINFO_SIZE_UPLOAD, &d);
	if( d > total )
		d = total;
	if( d <= 0 )
		d = 1;
	return (size_t)d;
};
