/*
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * March 2010: TightVNC 1.3.10 source modified as standalone x86/x64 DLL.
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * Tested On x86 Windows: - NT4, 2000, XP, 2003, Vista, 2008, 7
 * Tested On x64 Windows: - 2003, 2008R2, 7
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 */
#include "stdhdrs.h"
#include "common.h"
#include "vncServer.h"

HINSTANCE hAppInstance = NULL;
/*
 * Use Reflective DLL Injection.
 */
// #define REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR
// #define REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN
// #include "ReflectiveLoader.c"

/* init winsock */
void winsock_init()
{
	WSADATA	wsaData;
	WORD 	wVersionRequested;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
}

/*
 * Post an arbitrary message back to a loader.
 */
DWORD vncdll_postmessage( AGENT_CTX * lpAgentContext, DWORD dwMessage, BYTE * pDataBuffer, DWORD dwDataLength )
{
	/* there is no loader. We do not want these messages */
	return 0;
}

/*
 * 
 */
VOID vncdll_unlockwindowstation( VOID )
{
	HMODULE hUser32 = LoadLibrary( "user32.dll" );
	if( hUser32 )
	{
		typedef BOOL (WINAPI * UNLOCKWINDOWSTATION)( HWINSTA );
		UNLOCKWINDOWSTATION pUnlockWindowStation = (UNLOCKWINDOWSTATION)GetProcAddress( hUser32, "UnlockWindowStation" );
		if( pUnlockWindowStation )
			pUnlockWindowStation( GetProcessWindowStation() );

		FreeLibrary( hUser32 );
	}
}

/*
 * Switch to the input desktop and set it as this threads desktop.
 */
HDESK vncdll_getinputdesktop( BOOL bSwitchStation )
{
	// grab a handle to the default input desktop (e.g. Default or WinLogon)
	return OpenInputDesktop( 0, FALSE, MAXIMUM_ALLOWED );
}

/*
 * Create the Metasploit Courtesy Shell
 */
VOID vncdll_courtesyshell( HDESK desk )
{
	/* absolutely not! */
}

/*
 * Create and run a VNC server on this socket.
 */
DWORD vncdll_run( AGENT_CTX * lpAgentContext )
{
	DWORD dwResult      = ERROR_SUCCESS;
	VSocketSystem * vsocketsystem = NULL;
	vncServer * vserver = NULL;
	HDESK desk          = NULL;
	WSADATA WSAData     = {0};
	SOCKET sock         = 0;
	BYTE bFlags         = 0;

	do
	{
		dprintf( "[VNCDLL] vncdll_run. Started..." );

		if( !lpAgentContext )
			BREAK_WITH_ERROR( "[VNCDLL] vncdll_run. invalid parameters", ERROR_INVALID_PARAMETER );
		
		desk = vncdll_getinputdesktop( TRUE );
		
		vncdll_unlockwindowstation();

		vsocketsystem = new VSocketSystem();
		if( !vsocketsystem->Initialised() )
			BREAK_WITH_ERROR( "[VNCDLL] vncdll_run. VSocketSystem Initialised failed", ERROR_NETWORK_ACCESS_DENIED );

		vserver = new vncServer();

		vncClientId cid = vserver->AddClient( lpAgentContext );

		dprintf( "[VNCDLL-0x%08X] vncdll_run. Going into wait state... cid=%d", hAppInstance, cid );
	
		WaitForSingleObject( lpAgentContext->hCloseEvent, INFINITE );
		
		vserver->RemoveClient( cid );

	} while( 0 );

	dprintf( "[VNCDLL-0x%08X] vncdll_run. terminating...", hAppInstance );

	delete vserver;

	delete vsocketsystem;

	return 0;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	if (__argc < 3){
		return 1;
	}
    hAppInstance = hInstance;
	winsock_init();
 	
	SOCKET lsocket;
	lsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (lsocket == INVALID_SOCKET){
		return 2;
	}
	struct sockaddr_in sock;
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = inet_addr(__argv[1]);	/* we're a localhost only VNC server */
	sock.sin_port = htons(atoi(__argv[2]));	/* grab the port off of the CLI */
	if (bind(lsocket, (sockaddr *)&sock, sizeof(sock)) == SOCKET_ERROR){
		return 3;
	}
	if (listen(lsocket, 0) == SOCKET_ERROR){
		return 4;
	}
	while(true){
		SOCKET my_socket;
		my_socket = accept(lsocket, 0, 0);
		if(my_socket == INVALID_SOCKET){
			continue;
		}
		AGENT_CTX context = {0};
		/* setup our agent context, please */
		WSADuplicateSocket(my_socket, GetCurrentProcessId(), &context.info);
		context.hCloseEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		context.bInit       = TRUE;
		vncdll_run( &context );
		closesocket(my_socket);
	}
    return 0;
}