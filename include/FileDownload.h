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


// File downloading over HTTP
// Created 13/10/07
// By Karel Petranek, Albert Zeyer and Martin Griffin


#ifndef __FILEDOWNLOAD_H__
#define __FILEDOWNLOAD_H__

#include <list>
#include <vector>
#include <string>
#include <map>
#include <stdio.h>  // for FILE
#include "ThreadPool.h"
#include <SDL_mutex.h>
#include "HTTP.h"
#include "olx-types.h"
#include "CBytestream.h"


// File download states
enum {
	FILEDL_NO_FILE = -1,
	FILEDL_INITIALIZING = 0,
	FILEDL_RECEIVING,
	FILEDL_FINISHED,
	FILEDL_ERROR
};

// File download errors
// NOTE: if you change this, change also error strings in FileDownload.cpp
enum  {
	FILEDL_ERROR_NO_ERROR = 0,
	FILEDL_ERROR_NO_FILE,
	FILEDL_ERROR_NO_DEST,
	FILEDL_ERROR_NO_SERVER,
	FILEDL_ERROR_HTTP,
	FILEDL_ERROR_SAVING,
};

// File download error structure
class DownloadError  { public:
	std::string	sErrorMsg;
	int			iError;
	HttpError	tHttpError;

	DownloadError& operator= (const DownloadError& oth)  {
		if (&oth != this)  {
			sErrorMsg = oth.sErrorMsg;
			iError = oth.iError;
			tHttpError = oth.tHttpError;
		}
		return *this;
	}
};


// Single file download handling class
class CHttpDownloader  {
public:
	// Constructors and destructors
	CHttpDownloader() : 
		tFile(NULL),
		tDownloadServers(NULL),
		iState(FILEDL_NO_FILE),
		iID(0)
		{ tMutex = SDL_CreateMutex(); }

	CHttpDownloader(std::vector<std::string> *download_servers, size_t id) :
		tFile(NULL),
		tDownloadServers(download_servers),
		iState(FILEDL_NO_FILE),
		iID(id)
		{ tMutex = SDL_CreateMutex(); }

	~CHttpDownloader()  { Stop(); SDL_DestroyMutex(tMutex); }


private:
	CHttpDownloader(const CHttpDownloader&) { assert(false); }
	CHttpDownloader& operator=(const CHttpDownloader& dl) {
		assert(false); /*
		 if (&dl == this)
		 return *this;
		 
		 sFileName = dl.sFileName;
		 sDestPath = dl.sDestPath;
		 tFile = dl.tFile;
		 tDownloadServers = dl.tDownloadServers; // HINT: this is a pointer
		 iCurrentServer = dl.iCurrentServer;
		 iState = dl.iState;
		 iID = dl.iID;
		 tHttp = dl.tHttp; // HINT: safe, CHttp has a copy operator defined
		 tError = dl.tError; */
		 return *this;
	}
	
	std::string		sFileName;
	std::string		sDestPath;
	FILE			*tFile;
	std::vector<std::string> *tDownloadServers;
	int				iCurrentServer;
	int				iState;
	size_t			iID;
	CHttp			tHttp;
	DownloadError	tError;
	SDL_mutex		*tMutex;

private:
	void			SetHttpError(HttpError err);
	void			SetDlError(int id);

public:
	void				Start(const std::string& filename, const std::string& dest_dir);
	void				Stop();
	void				ProcessDownload();
	std::string			GetFileName();
	int					GetState();
	size_t				GetID();
	int					GetProgress();
	DownloadError		GetError();
	void				Lock()			{ SDL_LockMutex(tMutex); }
	void				Unlock()		{ SDL_UnlockMutex(tMutex); }
};


// File downloader class (multiple downloads at once, a set of CFileDownload)
class CHttpDownloadManager  {
public:
	CHttpDownloadManager();
	~CHttpDownloadManager();

private:
	std::list<CHttpDownloader *> tDownloads;
	std::vector<std::string>	 tDownloadServers;
	SmartPointer<ThreadPoolItem>	tThread;
	SDL_mutex					*tMutex;
	size_t						 iActiveDownloads;

	friend Result ManagerMain(void *param);
	volatile bool				bBreakThread;

public:
	void						ProcessDownloads();
	void						StartFileDownload(const std::string& filename, const std::string& dest_dir);
	void						CancelFileDownload(const std::string& filename);
	void						RemoveFileDownload(const std::string& filename);
	void						RemoveAllDownloads();
	bool						IsFileDownloaded(const std::string& filename);
	DownloadError				FileDownloadError(const std::string& filename);
	int							GetFileProgress(const std::string& filename);
	void						Lock()	 { SDL_LockMutex(tMutex); }
	void						Unlock()	{ SDL_UnlockMutex(tMutex); }
	std::list<CHttpDownloader *> *GetDownloads()		{ return &tDownloads; }
};

// In-lobby or in-game file downloader over unreliable protocol - send packets of 256 bytes
// Actually we're using reliable CChannel to send packets so this downloader
// doesn't contain any checks on packet lost / received in wrong order, 
// only Adler32 checksum which is calculated by zlib.
class CUdpFileDownloader
{
public:
	CUdpFileDownloader() { reset(); bAllowFileRequest = true; };
	~CUdpFileDownloader() { };

	enum State_t 	{ S_SEND, S_RECEIVE, S_FINISHED };

	// Basic functions for file download
	const std::string & getFilename() const { return isReceiving() ? sLastFileRequested : sFilename; };
	// getData() contains garbage when download not finished yet, or when uploading a file
	const std::string & getData() const { return sData; } ;

	// Should be called when received S2C_SENDFILE or C2S_SENDFILE msg - read needed amount of bytes from bytestream
	// Returns true if download finished or error occured
	// If file request received automatically start sending requested file if bAllowFileRequest is true
	bool		receive( CBytestream * bs );
	// Should be called to append file data to S2C_SENDFILE or C2S_SENDFILE messages when sending in a loop
	// Returns true if download finished or error occured
	bool		send( CBytestream * bs );

	// Does not change any variables, just pings server with zero-sized packet to un-freeze it, else download speed sucks
	void		sendPing( CBytestream * bs ) const;

	State_t		getState() const { return tState; };

	bool		isSending() const { return tState == S_SEND; };
	bool		isReceiving() const { return tState == S_RECEIVE; };
	bool		isFinished() const { return tState == S_FINISHED; };
	bool		wasSending() const { return tPrevState == S_SEND; };
	bool		wasReceiving() const { return tPrevState == S_RECEIVE; };

	bool		wasError() const { return bWasError; };
	void		clearError() { bWasError = false; };

	bool		wasAborted() const { return bWasAborted; };
	void		clearAborted() { bWasAborted = false; };

	void		setDataToSend( const std::string & name, const std::string & data, bool noCompress = false );
	void		setFileToSend( const std::string & path );

	void		reset();
	
	void		abortDownload();	// Aborts both downloading and uploading, data to send is in one packet less than 256 bytes

	// Functions that will trigger remote CFileDownloaderInGame to do something like send some file or list some dir
	void		allowFileRequest( bool allow );
	void		requestFile( const std::string & path, bool retryIfFail );
	bool		requestFilesPending(); // Re-send file request if downloading fails
	void		removeFileFromRequest( const std::string & path );
	static bool	isPathValid( const std::string & path );	// Check if someone tries to access /etc/shadow to get system passwords
	
	struct		StatInfo
	{
		std::string filename;
		uint size;
		uint compressedSize;	// Approximate! Use only for progressbars
		uint checksum;
		StatInfo( const std::string & _filename="", uint _size=0, uint _compressedSize=0, uint _checksum=0 ):
			filename(_filename), size(_size), compressedSize(_compressedSize), checksum(_checksum) {};
	};
	
	// For dir returns recursive list of all files in dir, clears previous file info
	void		requestFileInfo( const std::string & path, bool retryIfFail ); 
	// File statistics from requestFileInfo() is saved in array returned by this func
	const std::vector< StatInfo > & getFileInfo() const { return cStatInfo; };
	
	// Additional functionality to show download progress - inexact, server may send any file and we should accept it,
	// yet current implementation will send only files we will request
	float		getFileDownloadingProgress() const; // Downloading progress of current file, in range 0.0-1.0
	size_t		getFileDownloadingProgressBytes() const;
	size_t		getFilesPendingAmount() const;
	size_t		getFilesPendingSize() const; // Calculates compressed size of all pending files, incluing the one currently downloading

private:
	void			processFileRequests();

	// TODO: should use intern-pointer here
	std::string		sFilename;
	std::string		sData;
	size_t			iPos;

	State_t			tPrevState;
	State_t			tState;
	bool			bWasError;
	bool			bWasAborted;
	
	bool			bAllowFileRequest;
	
	std::vector< std::string > tRequestedFiles;
	
	std::vector< StatInfo > cStatInfo;
	
	// For progressbar and download percentage
	std::map< std::string, StatInfo > cStatInfoCache;
	std::string sLastFileRequested;

};

#endif // __FILEDOWNLOAD_H__
