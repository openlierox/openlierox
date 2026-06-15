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


// HTTP class
// Created 9/4/02
// Jason Boettcher

#ifndef __HTTP_H__
#define __HTTP_H__

#include <string>

#include "Networking.h"
#include "Event.h"
#include "SmartPointer.h"
#include "Mutex.h"

#include "ThreadPool.h"

typedef void CURL;

//
// Functions
//
void AutoSetupHTTPProxy();

//
// Classes
//

// HTTP Post Field structure
class HTTPPostField  {
private:
	std::string sData;
	std::string sMimeType;
	std::string sName;
	std::string sFileName;
public:
	HTTPPostField() {}
	HTTPPostField(const std::string& data, const std::string& mimetype, const std::string& name, const std::string& file) :
		sData(data), sMimeType(mimetype), sName(name), sFileName(file) {}
	HTTPPostField(const HTTPPostField& oth)  { operator= (oth); }


	HTTPPostField& operator= (const HTTPPostField& oth)  {
		if (&oth == this)
			return *this;
		sData = oth.sData;
		sMimeType = oth.sMimeType;
		sName = oth.sName;
		sFileName = oth.sFileName;

		return *this;
	}

	void setFileName(const std::string& n)	{ sFileName = n; }
	const std::string& getFileName() const	{ return sFileName; }

	void setName(const std::string& n)	{ sName = n; }
	const std::string& getName() const	{ return sName; }

	void setMimeType(const std::string& n)	{ sMimeType = n; }
	const std::string& getMimeType() const	{ return sMimeType; }

	void setData(const std::string& d)	{ sData = d; }
	const std::string& getData() const	{ return sData; }
};

// HTTP error struct
class HttpError  { public:
	std::string sErrorMsg;
	int			iError;

	HttpError& operator=(const HttpError& oth)  {
		if (&oth != this)  {
			sErrorMsg = oth.sErrorMsg;
			iError = oth.iError;
		}
		return *this;
	}
};

// HTTP errors
// NOTE: If you change this, change also error strings in HTTP.cpp!!
enum  {
	HTTP_NO_ERROR = 0,
	HTTP_NO_SOCKET_ERROR,
	HTTP_CANNOT_RESOLVE_DNS,
	HTTP_INVALID_URL,
	HTTP_DNS_TIMEOUT,
	HTTP_ERROR_SENDING_REQ,
	HTTP_NO_CONNECTION,
	HTTP_ERROR_TIMEOUT,
	HTTP_NET_ERROR,
	HTTP_BAD_RESPONSE,
	HTTP_FILE_NOT_FOUND = 404
	// HINT: Add more if you need them
};

// HTTP processing results
enum HttpProc_t {
	HTTP_PROC_ERROR = -1,
	HTTP_PROC_PROCESSING = 0,
	HTTP_PROC_FINISHED = 1
};



struct CurlThread;

class CHttp {
public:
	enum Action  {
		htaGet = 0,
		htaHead,
		htaPost
	};
	
	struct HttpEventData  {
		HttpEventData(CHttp *h, bool succeeded) : cHttp(h), bSucceeded(succeeded) {}
		CHttp *cHttp;
		bool bSucceeded;
	};
		
	
	CHttp();
	~CHttp();

	Event<HttpEventData>	onFinished;
	
	//void				SendSimpleData(const std::string& data, const std::string url, const std::string& proxy = "");
	void				SendData(const std::list<HTTPPostField>& data, const std::string url, const std::string& proxy = "");
	void				RequestData(const std::string& url, const std::string& proxy = "");
	HttpProc_t			ProcessRequest()			{ Mutex::ScopedLock l(Lock); return ProcessingResult; };
	void				CancelProcessing();
	void				ClearReceivedData()			{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); Data = ""; }
	HttpError			GetError() const			{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return Error; }
	long				GetHTTPStatusCode() const	{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return HTTPStatusCode; }
	const std::string&	GetData() const				{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return curlThread != NULL ? Empty : Data; }
	std::string			GetMimeType() const;
	//const std::string&	GetDataToSend() const		{ Mutex::ScopedLock l(Lock); return DataToSend; }
	size_t				GetDataToSendLength() const;
	size_t				GetDataLength() const;
	size_t				GetReceivedDataLen() const	{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return Data.size(); }
	size_t				GetSentDataLen() const;
	bool				RequestedData()	const		{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return curlThread != NULL; }

	TimeDiff			GetDownloadTime() const		{ Mutex::ScopedLock l(const_cast<Mutex &>(Lock)); return DownloadEnd - DownloadStart; }

	float				GetDownloadSpeed() const;
	float				GetUploadSpeed() const;

	std::string			GetHostName() const;
	const std::string&	GetUrl() const			{ return Url; }
	bool				IsRedirecting() const	{ return false; }


private:
	// they are not allowed atm
	CHttp(const CHttp& oth)  { operator= (oth); }
	CHttp& operator=(const CHttp& http);


	CURL *			InitializeTransfer(const std::string& url, const std::string& proxy);
	
	// All string data passed to libcurl should be not freed until curl_easy_cleanup, that's fixed in libcurl 7.17.0 (Sep 2007)
	std::string		Url;
	std::string		Proxy;
	std::string		Useragent;
	
	std::string		Data;  // Data received from the network

	HttpProc_t		ProcessingResult;
	HttpError		Error;
	long			HTTPStatusCode;	// HTTP status code of the (final) response, 0 if unavailable

	std::string		Empty; // Returned when Data is being processed

	friend struct CurlThread;
	CurlThread *	curlThread;
	Mutex			Lock;

	// Bandwidth measurement
	AbsTime			DownloadStart;
	AbsTime			DownloadEnd;
	
};

#endif  // __HTTP_H__
