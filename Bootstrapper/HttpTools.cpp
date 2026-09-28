#include "stdafx.h"
#include "HttpTools.h"
#include "SharedHelpers.h"
#include "atlutil.h"
#include "wininet.h"
#include <strstream>
#include <sstream>
#include <cstdlib>
#pragma comment (lib, "Wininet.lib")

namespace HttpTools
{
	class WININETHINTERNET
	{
		HINTERNET handle;
		WININETHINTERNET(const WININETHINTERNET&);
		WININETHINTERNET& operator=(const WININETHINTERNET&);
	public:
		WININETHINTERNET(HINTERNET handle):handle(handle) {}
		WININETHINTERNET():handle(0) {}
		WININETHINTERNET& operator = (HINTERNET handle)
		{
			::InternetCloseHandle(this->handle);
			this->handle = handle;
			return *this;
		}
		operator bool() { return handle!=0; }
		operator HINTERNET() { return handle; }
		~WININETHINTERNET()
		{
			::InternetCloseHandle(handle);
		}
	};

	int httpWinInet(IInstallerSite *site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress);
	int http(IInstallerSite *site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress, bool log = true);

	static void dummyProgress(int, int) {}
	const std::string getPrimaryCdnHost(IInstallerSite *site)
	{
		static std::string cdnHost;
		static bool cdnHostLoaded = false;
		static bool validCdnHost = false;

		if(!cdnHostLoaded)
		{
			if (site->ReplaceCdnTxt()) {
				std::string host = site->InstallHost();
				std::string prod ("setup.karblox.cc");
				if (host.compare(prod) == 0) {
					cdnHost = "setup.karblox.cc";
					validCdnHost = true;
					LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
				} else {
					cdnHost = host;
					validCdnHost = true;
					LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
				}
			} else {
				try
				{
					std::ostrstream result;
					int status_code;
					switch(status_code = httpGet(site, site->InstallHost(), "/cdn.txt", std::string(), result, false, dummyProgress))
					{
						case 200:
						case 304:
							result << (char) 0;
							cdnHost = result.str();
							LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
							validCdnHost = true;
							break;
						default:
							validCdnHost = false;
							LLOG_ENTRY1(site->Logger(), "primaryCdn failure code=%d, falling back to secondary installHost", status_code);
							break;
					}
				}
				catch(std::exception&)
				{
					//Quash exceptions, set validCdnHost to false
					validCdnHost = false;
					LLOG_ENTRY(site->Logger(), "primaryCdn exception, falling back to secondary installHost");
				}
			}
			
			//Only try to load the CDN once, then give up
			cdnHostLoaded = true;
		}

		return cdnHost;
	}

	const std::string getCdnHost(IInstallerSite *site)
	{
		static std::string cdnHost;
		static bool cdnHostLoaded = false;
		static bool validCdnHost = false;

		if(!cdnHostLoaded)
		{
			try
			{
				std::ostrstream result;
				int status_code;
				switch(status_code = httpGet(site, site->BaseHost(), "/install/GetInstallerCdns.ashx", std::string(), result, false, dummyProgress))
				{
				case 200:
				case 304:
				{
					result << (char) 0;
					LLOG_ENTRY1(site->Logger(), "primaryCdns: %s", result.str());

					// Minimal flat-JSON parse: {"cdnHost": weight, ...}.
					// No JSON library (standalone build) - scan "key":number pairs.
					std::vector<std::pair<std::string, int>> cdns;
					int totalValue = 0;
					{
						const std::string json = result.str();
						std::string::size_type pos = 0;
						while ((pos = json.find((char)34, pos)) != std::string::npos)
						{
							std::string::size_type end = json.find((char)34, pos + 1);
							if (end == std::string::npos)
								break;
							std::string key = json.substr(pos + 1, end - pos - 1);
							std::string::size_type colon = json.find((char)58, end + 1);
							if (colon == std::string::npos)
								break;
							int value = atoi(json.c_str() + colon + 1);
							if (cdns.size() > 0)
								cdns.push_back(std::make_pair(key, cdns.back().second + value));
							else
								cdns.push_back(std::make_pair(key, value));

							totalValue += value;
							pos = colon + 1;
						}
					}

					if (cdns.size() && (totalValue > 0))
					{
						// randomly pick a cdn
						int r = rand() % totalValue;
						for (unsigned int i = 0; i < cdns.size(); i++)
						{
							if (r < cdns[i].second)
							{
								cdnHost = cdns[i].first;
								validCdnHost = true;
								break;
							}
						}
					}

					break;
				}
				default:
					validCdnHost = false;
					LLOG_ENTRY1(site->Logger(), "primaryCdn failure code=%d, falling back to secondary installHost", status_code);
					break;
				}
			}
			catch(std::exception&)
			{
				LLOG_ENTRY(site->Logger(), "primaryCdn exception, falling back to secondary installHost");
			}
			//Only try to load the CDN once, then give up
			cdnHostLoaded = true;
		}

		return cdnHost;
	}

	class CInternet
	{
		HINTERNET handle;
		CInternet(const CInternet&);
		CInternet& operator=(const CInternet&);
	public:
		CInternet(HINTERNET handle):handle(handle) {}
		CInternet():handle(0) {}
		CInternet& operator = (HINTERNET handle)
		{
			::InternetCloseHandle(this->handle);
			this->handle = handle;
			return *this;
		}
		operator bool() { return handle!=0; }
		operator HINTERNET() { return handle; }
		~CInternet()
		{
			::InternetCloseHandle(handle);
		}
	};

	class Buffer
	{
		Buffer(const Buffer&);
		Buffer& operator=(const Buffer&);
		void* const data;
	public:
		Buffer(size_t size):data(malloc(size)) {}
		~Buffer() { free(data); }
		operator const void*() const { return data; }
		operator void*() { return data; }
		operator char*() { return (char*)data; }
	};

	int httpGet(IInstallerSite *site, std::string host, std::string path, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress, bool log)
	{
		try
		{
			std::string tmp = etag;
			std::strstream input;
			int i = http(site, "GET", host, path, input, NULL, tmp, result, ignoreCancel, progress, log);
			etag = tmp;
			return i;
		}
		catch (silent_exception&)
		{
			throw;
		}
		catch (std::exception& e)
		{
			LLOG_ENTRY2(site->Logger(), "WARNING: First HTTP GET error for %s: %s", path.c_str(), exceptionToString(e).c_str());
			std::strstream input;
			result.seekp(0);
			result.clear();
			// try again
			return http(site, "GET", host, path, input, NULL, etag, result, ignoreCancel, progress, log);
		}
	}

	std::string httpGetString(const std::string& url)
	{
		CUrl u;
		u.CrackUrl(convert_s2w(url).c_str());

		const bool isSecure = u.GetScheme() == ATL_URL_SCHEME_HTTPS;

		// Initialize the User Agent
		WININETHINTERNET session = InternetOpen(L"Karblox/WinInet", PRE_CONFIG_INTERNET_ACCESS, NULL, NULL, 0);
		if (!session) 
		{
			throw std::runtime_error("httpGetString - InternetOpen ERROR");
		}

		WININETHINTERNET connection = ::InternetConnect(session, u.GetHostName(), u.GetPortNumber(), u.GetUserName(), u.GetPassword(), INTERNET_SERVICE_HTTP, 0, 0); 
		if (!connection) 
		{
			throw std::runtime_error("httpGetString - InternetConnect ERROR");
		}

		CString s = u.GetUrlPath();
		s += u.GetExtraInfo();
		WININETHINTERNET request = ::HttpOpenRequest(connection, _T("GET"), s, HTTP_VERSION, _T(""), NULL, isSecure ? INTERNET_FLAG_SECURE : 0, 0); 
		if (!request) 
		{
			throw std::runtime_error("httpGetString - HttpOpenRequest ERROR");
		}

		DWORD httpSendResult = ::HttpSendRequest(request, NULL, 0, 0, 0);
		if (!httpSendResult) 
		{
			throw std::runtime_error("httpGetString - HttpSendRequest ERROR");
		}

		std::ostringstream data;
		while (true)
		{
			DWORD numBytes;
			if (!::InternetQueryDataAvailable(request, &numBytes, 0, 0))
			{
				DWORD err = GetLastError();
				if(err == ERROR_IO_PENDING)
				{
					Sleep(100); // we block on data.
					continue;
				}
				else
				{
					throw std::runtime_error("httpGetString - InternetQueryDataAvailable ERROR");
				}
			}

			if (numBytes==0)
				break; // EOF

			if (numBytes == -1)
			{
				throw std::runtime_error("httpGetString - No Settings ERROR");
			}

			char* buffer = (char*)malloc(numBytes + 1);
			DWORD bytesRead;
			throwLastError(::InternetReadFile(request, (LPVOID) buffer, numBytes, &bytesRead), "InternetReadFile failed");
			data.write(buffer, bytesRead);
			free(buffer);
		}

		return data.str();
	}

	int httpWinInet(IInstallerSite *site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress)
	{
		CUrl u;
		BOOL urlCracked;
		#ifdef UNICODE
			urlCracked = u.CrackUrl(convert_s2w(host).c_str());
		#else
			urlCracked = u.CrackUrl(host.c_str());
		#endif

		// Initialize the User Agent
		CInternet session = InternetOpen(_T("Karblox/WinInet"), PRE_CONFIG_INTERNET_ACCESS, NULL, NULL, 0);
		if (!session)
			throw std::runtime_error(format_string("InternetOpen failed for %s http://%s%s, Error Code: %d", method, host.c_str(), path.c_str(), GetLastError()).c_str());

		CInternet connection;
		if (urlCracked)
			connection = ::InternetConnect(session, u.GetHostName(), u.GetPortNumber(), NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		else
			connection = ::InternetConnect(session, convert_s2w(host).c_str(), 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

		if (!connection)
			throw std::runtime_error(format_string("InternetConnect failed for %s http://%s%s, Error Code: %d, Port Number: %d", method, host.c_str(), path.c_str(), GetLastError() , urlCracked ? u.GetPortNumber() : 80).c_str());

		//   1. Open HTTP Request (pass method type [get/post/..] and URL path (except server name))
		CInternet request = ::HttpOpenRequest(
			connection, convert_s2w(method).c_str(), convert_s2w(path).c_str(), NULL, NULL, NULL, 
			INTERNET_FLAG_KEEP_CONNECTION |
			INTERNET_FLAG_EXISTING_CONNECT |
			INTERNET_FLAG_NEED_FILE, // ensure that it gets cached
			1); 
		if (!request)
			throw std::runtime_error(format_string("HttpOpenRequest failed for %s http://%s%s, Error Code: %d", method, host.c_str(), path.c_str(), GetLastError()).c_str());

		if (contentType)
		{
			std::string header = format_string("Content-Type: %s\r\n", contentType);
			throwLastError(::HttpAddRequestHeaders(request, convert_s2w(header).c_str(), header.size(), HTTP_ADDREQ_FLAG_ADD), "HttpAddRequestHeaders failed");
		}

		size_t uploadSize;
		{
			size_t x = input.tellg();
			input.seekg (0, std::ios::end);
			size_t y = input.tellg();
			uploadSize = y - x;
			input.seekg (0, std::ios::beg);
		}

		if (uploadSize==0)
		{
			throwLastError(::HttpSendRequest(request, NULL, 0, 0, 0), "HttpSendRequest failed");
		}
		else
		{
			Buffer uploadBuffer(uploadSize);

			input.read((char*)uploadBuffer, uploadSize);

			// Send the request
			{
				INTERNET_BUFFERS buffer;
				memset(&buffer, 0, sizeof(buffer));
				buffer.dwStructSize = sizeof(buffer);
				buffer.dwBufferTotal = uploadSize;
				if (!HttpSendRequestEx(request, &buffer, NULL, 0, 0))
					throw std::runtime_error("HttpSendRequestEx failed");

				try
				{
					DWORD bytesWritten;
					throwLastError(::InternetWriteFile(request, uploadBuffer, uploadSize, &bytesWritten), "InternetWriteFile failed");

					if (bytesWritten!=uploadSize)
						throw std::runtime_error("Failed to upload content");
				}
				catch (std::exception&)
				{
					::HttpEndRequest(request, NULL, 0, 0);
					throw;
				}

				throwLastError(::HttpEndRequest(request, NULL, 0, 0), "HttpEndRequest failed");
			}
		}

		// Check the return HTTP Status Code
		DWORD statusCode;
		{
			DWORD dwLen = sizeof(DWORD);
			throwLastError(::HttpQueryInfo(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &dwLen, NULL), "HttpQueryInfo HTTP_QUERY_STATUS_CODE failed");
		}
		DWORD contentLength = 0;
		{
			DWORD dwLen = sizeof(DWORD);
			::HttpQueryInfo(request, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &dwLen, NULL);
		}
		{
			CHAR buffer[256];
			DWORD dwLen = sizeof(buffer);
			if (::HttpQueryInfo(request, HTTP_QUERY_ETAG, &buffer, &dwLen, NULL))
			{
				etag = buffer;
				etag = etag.substr(1, etag.size()-2);	// remove the quotes
			}
			else
				etag.clear();
		}

		DWORD readSoFar = 0;
		while (true)
		{
			DWORD numBytes;
			if (!::InternetQueryDataAvailable(request, &numBytes, 0, 0))
				numBytes = 0;
			if (numBytes==0)
				break; // EOF

			char buffer[1024];
			DWORD bytesRead;
			throwLastError(::InternetReadFile(request, (LPVOID) buffer, 1024, &bytesRead), "InternetReadFile failed");
			result.write(buffer, bytesRead);
			readSoFar += bytesRead;
			progress(bytesRead, contentLength);
		}

		if (statusCode!=HTTP_STATUS_OK)
		{
			TCHAR buffer[512];
			DWORD length = 512;
			if (::HttpQueryInfo(request, HTTP_QUERY_STATUS_TEXT, (LPVOID) buffer, &length, 0))
				throw std::runtime_error(convert_w2s(buffer).c_str());
			else
				throw std::runtime_error(format_string("statusCode = %d", statusCode));
		}

		return statusCode;
	}


	int http(IInstallerSite *site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress, bool log)
	{
		if (log)
		{
			if (!etag.empty())
			{
				LLOG_ENTRY4(site->Logger(), "%s http://%s%s If-None-Match: \"%s\"", method, host.c_str(), path.c_str(), etag.c_str());
			}
			else
			{
				LLOG_ENTRY3(site->Logger(), "%s http://%s%s", method, host.c_str(), path.c_str());
			}
		}
		// Standalone build: WinInet is the only transport (no boost::asio).
		return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
	}

	int httpPost(IInstallerSite *site, std::string host, std::string path, std::istream& input, const char* contentType, std::ostream& result, bool ignoreCancel, ProgressCallback progress, bool log)
	{
		return http(site, "POST", host, path, input, contentType, std::string(), result, ignoreCancel, progress, log);
	}

	int httpGetCdn(IInstallerSite *site, std::string secondaryHost, std::string path, std::string& etag, std::ostream& result, bool ignoreCancel, ProgressCallback progress)
	{
		std::string cdnHost;
		if (site->UseNewCdn())
			cdnHost = getCdnHost(site);
		
		if (cdnHost.empty())
			cdnHost = getPrimaryCdnHost(site);
		
		if(!cdnHost.empty()){
			try
			{
				std::string tmp = etag;
				int status_code = httpGet(site, cdnHost, path, tmp, result, ignoreCancel, progress);
				switch(status_code){
					case 200:
					case 304:
						//We succeeded so save the etag and return success
						etag = tmp;
						return status_code;
					default:
						LLOG_ENTRY3(site->Logger(), "Failure getting '%s' from cdnHost='%s', falling back to secondaryHost='%s'", path.c_str(), cdnHost.c_str(), secondaryHost.c_str());
						//Failure of some kind, fall back to secondaryHost below
						break;
				}
			}
			catch(std::exception&)
			{ 
				//Trap first exception and try again with secondaryHost
			}
		}

		//Reset our result vector
		result.seekp(0);
		result.clear();

		return httpGet(site, secondaryHost,path,etag,result,ignoreCancel,progress);
	}

}
