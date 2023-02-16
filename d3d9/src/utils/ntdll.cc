#include <stdio.h>
#include <windows.h>
#pragma comment(lib, "ntdll.lib")

EXTERN_C
{
    typedef enum _PS_ATTRIBUTE_NUM
    {
        PsAttributeParentProcess, // in HANDLE
        PsAttributeDebugObject, // in HANDLE
        PsAttributeToken, // in HANDLE
        PsAttributeClientId, // out PCLIENT_ID
        PsAttributeTebAddress, // out PTEB *
        PsAttributeImageName, // in PWSTR
        PsAttributeImageInfo, // out PSECTION_IMAGE_INFORMATION
        PsAttributeMemoryReserve, // in PPS_MEMORY_RESERVE
        PsAttributePriorityClass, // in UCHAR
        PsAttributeErrorMode, // in ULONG
        PsAttributeStdHandleInfo, // 10, in PPS_STD_HANDLE_INFO
        PsAttributeHandleList, // in HANDLE[]
        PsAttributeGroupAffinity, // in PGROUP_AFFINITY
        PsAttributePreferredNode, // in PUSHORT
        PsAttributeIdealProcessor, // in PPROCESSOR_NUMBER
        PsAttributeUmsThread, // ? in PUMS_CREATE_THREAD_ATTRIBUTES
        PsAttributeMitigationOptions, // in PPS_MITIGATION_OPTIONS_MAP (PROCESS_CREATION_MITIGATION_POLICY_*) // since WIN8
        PsAttributeProtectionLevel, // in PS_PROTECTION // since WINBLUE
        PsAttributeSecureProcess, // in PPS_TRUSTLET_CREATE_ATTRIBUTES, since THRESHOLD
        PsAttributeJobList, // in HANDLE[]
        PsAttributeChildProcessPolicy, // 20, in PULONG (PROCESS_CREATION_CHILD_PROCESS_*) // since THRESHOLD2
        PsAttributeAllApplicationPackagesPolicy, // in PULONG (PROCESS_CREATION_ALL_APPLICATION_PACKAGES_*) // since REDSTONE
        PsAttributeWin32kFilter, // in PWIN32K_SYSCALL_FILTER
        PsAttributeSafeOpenPromptOriginClaim, // in
        PsAttributeBnoIsolation, // in PPS_BNO_ISOLATION_PARAMETERS // since REDSTONE2
        PsAttributeDesktopAppPolicy, // in PULONG (PROCESS_CREATION_DESKTOP_APP_*)
        PsAttributeChpe, // in BOOLEAN // since REDSTONE3
        PsAttributeMitigationAuditOptions, // in PPS_MITIGATION_AUDIT_OPTIONS_MAP (PROCESS_CREATION_MITIGATION_AUDIT_POLICY_*) // since 21H1
        PsAttributeMachineType, // in WORD // since 21H2
        PsAttributeComponentFilter,
        PsAttributeEnableOptionalXStateFeatures, // since WIN11
        PsAttributeMax
    } PS_ATTRIBUTE_NUM;

#define RTL_USER_PROCESS_PARAMETERS_NORMALIZED              0x01
#define PS_ATTRIBUTE_NUMBER_MASK    0x0000ffff
#define PS_ATTRIBUTE_THREAD         0x00010000 // Attribute may be used with thread creation
#define PS_ATTRIBUTE_INPUT          0x00020000 // Attribute is input only
#define PS_ATTRIBUTE_ADDITIVE       0x00040000 // Attribute may be "accumulated", e.g. bitmasks, counters, etc.

#define PsAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & PS_ATTRIBUTE_NUMBER_MASK) | \
    ((Thread) ? PS_ATTRIBUTE_THREAD : 0) | \
    ((Input) ? PS_ATTRIBUTE_INPUT : 0) | \
    ((Additive) ? PS_ATTRIBUTE_ADDITIVE : 0))

#define PS_ATTRIBUTE_PARENT_PROCESS \
    PsAttributeValue(PsAttributeParentProcess, FALSE, TRUE, TRUE) // 0x60000
#define PS_ATTRIBUTE_DEBUG_OBJECT \
    PsAttributeValue(PsAttributeDebugObject, FALSE, TRUE, TRUE) // 0x60001
#define PS_ATTRIBUTE_TOKEN \
    PsAttributeValue(PsAttributeToken, FALSE, TRUE, TRUE) // 0x60002
#define PS_ATTRIBUTE_CLIENT_ID \
    PsAttributeValue(PsAttributeClientId, TRUE, FALSE, FALSE) // 0x10003
#define PS_ATTRIBUTE_TEB_ADDRESS \
    PsAttributeValue(PsAttributeTebAddress, TRUE, FALSE, FALSE) // 0x10004
#define PS_ATTRIBUTE_IMAGE_NAME \
    PsAttributeValue(PsAttributeImageName, FALSE, TRUE, FALSE) // 0x20005
#define PS_ATTRIBUTE_IMAGE_INFO \
    PsAttributeValue(PsAttributeImageInfo, FALSE, FALSE, FALSE) // 0x6
#define PS_ATTRIBUTE_MEMORY_RESERVE \
    PsAttributeValue(PsAttributeMemoryReserve, FALSE, TRUE, FALSE) // 0x20007
#define PS_ATTRIBUTE_PRIORITY_CLASS \
    PsAttributeValue(PsAttributePriorityClass, FALSE, TRUE, FALSE) // 0x20008
#define PS_ATTRIBUTE_ERROR_MODE \
    PsAttributeValue(PsAttributeErrorMode, FALSE, TRUE, FALSE) // 0x20009
#define PS_ATTRIBUTE_STD_HANDLE_INFO \
    PsAttributeValue(PsAttributeStdHandleInfo, FALSE, TRUE, FALSE) // 0x2000A
#define PS_ATTRIBUTE_HANDLE_LIST \
    PsAttributeValue(PsAttributeHandleList, FALSE, TRUE, FALSE) // 0x2000B
#define PS_ATTRIBUTE_GROUP_AFFINITY \
    PsAttributeValue(PsAttributeGroupAffinity, TRUE, TRUE, FALSE) // 0x2000C
#define PS_ATTRIBUTE_PREFERRED_NODE \
    PsAttributeValue(PsAttributePreferredNode, FALSE, TRUE, FALSE) // 0x2000D
#define PS_ATTRIBUTE_IDEAL_PROCESSOR \
    PsAttributeValue(PsAttributeIdealProcessor, TRUE, TRUE, FALSE) // 0x2000E
#define PS_ATTRIBUTE_MITIGATION_OPTIONS \
    PsAttributeValue(PsAttributeMitigationOptions, FALSE, TRUE, FALSE) // 0x60010
#define PS_ATTRIBUTE_PROTECTION_LEVEL \
    PsAttributeValue(PsAttributeProtectionLevel, FALSE, TRUE, FALSE) // 0x20011
#define PS_ATTRIBUTE_SECURE_PROCESS \
    PsAttributeValue(PsAttributeSecureProcess, FALSE, TRUE, FALSE) // 0x20012
#define PS_ATTRIBUTE_JOB_LIST \
    PsAttributeValue(PsAttributeJobList, FALSE, TRUE, FALSE) // 0x20013
#define PS_ATTRIBUTE_CHILD_PROCESS_POLICY \
    PsAttributeValue(PsAttributeChildProcessPolicy, FALSE, TRUE, FALSE) // 0x20014
#define PS_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY \
    PsAttributeValue(PsAttributeAllApplicationPackagesPolicy, FALSE, TRUE, FALSE) // 0x20015
#define PS_ATTRIBUTE_WIN32K_FILTER \
    PsAttributeValue(PsAttributeWin32kFilter, FALSE, TRUE, FALSE) // 0x20016
#define PS_ATTRIBUTE_SAFE_OPEN_PROMPT_ORIGIN_CLAIM \
    PsAttributeValue(PsAttributeSafeOpenPromptOriginClaim, FALSE, TRUE, FALSE) // 0x20017
#define PS_ATTRIBUTE_BNO_ISOLATION \
    PsAttributeValue(PsAttributeBnoIsolation, FALSE, TRUE, FALSE) // 0x20018
#define PS_ATTRIBUTE_DESKTOP_APP_POLICY \
    PsAttributeValue(PsAttributeDesktopAppPolicy, FALSE, TRUE, FALSE) // 0x20019
#define PS_ATTRIBUTE_CHPE \
    PsAttributeValue(PsAttributeChpe, FALSE, TRUE, TRUE) // 0x6001A
#define PS_ATTRIBUTE_MITIGATION_AUDIT_OPTIONS \
    PsAttributeValue(PsAttributeMitigationAuditOptions, FALSE, TRUE, FALSE) // 0x2001B
#define PS_ATTRIBUTE_MACHINE_TYPE \
    PsAttributeValue(PsAttributeMachineType, FALSE, TRUE, TRUE) // 0x6001C
#define PS_ATTRIBUTE_COMPONENT_FILTER \
    PsAttributeValue(PsAttributeComponentFilter, FALSE, TRUE, FALSE) // 0x2001D
#define PS_ATTRIBUTE_ENABLE_OPTIONAL_XSTATE_FEATURES \
    PsAttributeValue(PsAttributeEnableOptionalXStateFeatures, TRUE, TRUE, FALSE) // 0x3001E

    typedef struct _PS_ATTRIBUTE
    {
        ULONG_PTR Attribute;                // PROC_THREAD_ATTRIBUTE_XXX | PROC_THREAD_ATTRIBUTE_XXX modifiers, see ProcThreadAttributeValue macro and Windows Internals 6 (372)
        SIZE_T Size;                        // Size of Value or *ValuePtr
        union
        {
            ULONG_PTR Value;                // Reserve 8 bytes for data (such as a Handle or a data pointer)
            PVOID ValuePtr;                 // data pointer
        };
        PSIZE_T ReturnLength;               // Either 0 or specifies size of data returned to caller via "ValuePtr"
    } PS_ATTRIBUTE, *PPS_ATTRIBUTE;

    typedef enum _PS_IFEO_KEY_STATE
    {
        PsReadIFEOAllValues,
        PsSkipIFEODebugger,
        PsSkipAllIFEO,
        PsMaxIFEOKeyStates
    } PS_IFEO_KEY_STATE, *PPS_IFEO_KEY_STATE;

    typedef enum _PS_CREATE_STATE
    {
        PsCreateInitialState,
        PsCreateFailOnFileOpen,
        PsCreateFailOnSectionCreate,
        PsCreateFailExeFormat,
        PsCreateFailMachineMismatch,
        PsCreateFailExeName, // Debugger specified
        PsCreateSuccess,
        PsCreateMaximumStates
    } PS_CREATE_STATE;

    typedef struct _PS_CREATE_INFO
    {
        SIZE_T Size;
        PS_CREATE_STATE State;
        union
        {
            // PsCreateInitialState
            struct
            {
                union
                {
                    ULONG InitFlags;
                    struct
                    {
                        UCHAR WriteOutputOnExit : 1;
                        UCHAR DetectManifest : 1;
                        UCHAR IFEOSkipDebugger : 1;
                        UCHAR IFEODoNotPropagateKeyState : 1;
                        UCHAR SpareBits1 : 4;
                        UCHAR SpareBits2 : 8;
                        USHORT ProhibitedImageCharacteristics : 16;
                    } s1;
                } u1;
                ACCESS_MASK AdditionalFileAccess;
            } InitState;

            // PsCreateFailOnSectionCreate
            struct
            {
                HANDLE FileHandle;
            } FailSection;

            // PsCreateFailExeFormat
            struct
            {
                USHORT DllCharacteristics;
            } ExeFormat;

            // PsCreateFailExeName
            struct
            {
                HANDLE IFEOKey;
            } ExeName;

            // PsCreateSuccess
            struct
            {
                union
                {
                    ULONG OutputFlags;
                    struct
                    {
                        UCHAR ProtectedProcess : 1;
                        UCHAR AddressSpaceOverride : 1;
                        UCHAR DevOverrideEnabled : 1; // From Image File Execution Options
                        UCHAR ManifestDetected : 1;
                        UCHAR ProtectedProcessLight : 1;
                        UCHAR SpareBits1 : 3;
                        UCHAR SpareBits2 : 8;
                        USHORT SpareBits3 : 16;
                    } s2;
                } u2;
                HANDLE FileHandle;
                HANDLE SectionHandle;
                ULONGLONG UserProcessParametersNative;
                ULONG UserProcessParametersWow64;
                ULONG CurrentParameterFlags;
                ULONGLONG PebAddressNative;
                ULONG PebAddressWow64;
                ULONGLONG ManifestAddress;
                ULONG ManifestSize;
            } SuccessState;
        };
    } PS_CREATE_INFO, *PPS_CREATE_INFO;
    typedef struct _UNICODE_STRING
    {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    } UNICODE_STRING, *PUNICODE_STRING;
    typedef const UNICODE_STRING* PCUNICODE_STRING;
    typedef struct _PS_ATTRIBUTE_LIST
    {
        SIZE_T TotalLength;                 // sizeof(PS_ATTRIBUTE_LIST)
        PS_ATTRIBUTE Attributes[10];         // Depends on how many attribute entries should be supplied to NtCreateUserProcess
    } PS_ATTRIBUTE_LIST, *PPS_ATTRIBUTE_LIST;
    typedef struct _CURDIR
    {
        UNICODE_STRING DosPath;
        HANDLE Handle;
    } CURDIR, *PCURDIR;
    typedef struct _RTL_DRIVE_LETTER_CURDIR
    {
        USHORT Flags;
        USHORT Length;
        ULONG TimeStamp;
        UNICODE_STRING DosPath;
    } RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

#define RTL_MAX_DRIVE_LETTERS 32
    typedef struct _RTL_USER_PROCESS_PARAMETERS
    {
        ULONG MaximumLength;
        ULONG Length;

        ULONG Flags;
        ULONG DebugFlags;

        HANDLE ConsoleHandle;
        ULONG ConsoleFlags;
        HANDLE StandardInput;
        HANDLE StandardOutput;
        HANDLE StandardError;

        CURDIR CurrentDirectory;
        UNICODE_STRING DllPath;
        UNICODE_STRING ImagePathName;
        UNICODE_STRING CommandLine;
        PWCHAR Environment;

        ULONG StartingX;
        ULONG StartingY;
        ULONG CountX;
        ULONG CountY;
        ULONG CountCharsX;
        ULONG CountCharsY;
        ULONG FillAttribute;

        ULONG WindowFlags;
        ULONG ShowWindowFlags;
        UNICODE_STRING WindowTitle;
        UNICODE_STRING DesktopInfo;
        UNICODE_STRING ShellInfo;
        UNICODE_STRING RuntimeData;
        RTL_DRIVE_LETTER_CURDIR CurrentDirectories[RTL_MAX_DRIVE_LETTERS];

        ULONG_PTR EnvironmentSize;
        ULONG_PTR EnvironmentVersion;
        PVOID PackageDependencyData;
        ULONG ProcessGroupId;
        ULONG LoaderThreads;
    } RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;
    typedef struct _OBJECT_ATTRIBUTES
    {
        ULONG Length;
        HANDLE RootDirectory;
        PUNICODE_STRING ObjectName;
        ULONG Attributes;
        PVOID SecurityDescriptor;
        PVOID SecurityQualityOfService;
    } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

    NTSYSAPI
        NTSTATUS
        NTAPI
        RtlDestroyProcessParameters(PRTL_USER_PROCESS_PARAMETERS ProcessParameters);

    NTSYSAPI
        BOOLEAN
        NTAPI
        RtlFreeHeap(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress);

    NTSYSAPI
        VOID
        NTAPI
        RtlInitUnicodeString(PUNICODE_STRING DestinationString, PWSTR SourceString);

    NTSYSAPI
        NTSTATUS
        NTAPI
        RtlCreateProcessParameters(
            PRTL_USER_PROCESS_PARAMETERS * 	ProcessParameters,
            PUNICODE_STRING 	ImagePathName,
            PUNICODE_STRING 	DllPath,
            PUNICODE_STRING 	CurrentDirectory,
            PUNICODE_STRING 	CommandLine,
            PWSTR 	Environment,
            PUNICODE_STRING 	WindowTitle,
            PUNICODE_STRING 	DesktopInfo,
            PUNICODE_STRING 	ShellInfo,
            PUNICODE_STRING 	RuntimeData
        );

    NTSYSCALLAPI
        NTSTATUS
        NTAPI
        NtCreateUserProcess(
            _Out_ PHANDLE ProcessHandle,
            _Out_ PHANDLE ThreadHandle,
            _In_ ACCESS_MASK ProcessDesiredAccess,
            _In_ ACCESS_MASK ThreadDesiredAccess,
            _In_opt_ POBJECT_ATTRIBUTES ProcessObjectAttributes,
            _In_opt_ POBJECT_ATTRIBUTES ThreadObjectAttributes,
            _In_ ULONG ProcessFlags,
            _In_ ULONG ThreadFlags,
            _In_ PRTL_USER_PROCESS_PARAMETERS ProcessParameters,
            _Inout_ PPS_CREATE_INFO CreateInfo,
            _In_ PPS_ATTRIBUTE_LIST AttributeList
        );

    NTSYSAPI
        PVOID
        NTAPI
        RtlAllocateHeap(
            _In_ PVOID HeapHandle,
            _In_opt_ ULONG Flags,
            _In_ SIZE_T Size
        );

    typedef struct _PS_STD_HANDLE_INFO
    {
        union
        {
            ULONG Flags;
            struct
            {
                ULONG StdHandleState : 2; // PS_STD_HANDLE_STATE
                ULONG PseudoHandleMask : 3; // PS_STD_*
            } s;
        };
        ULONG StdHandleSubsystemType;
    } PS_STD_HANDLE_INFO, *PPS_STD_HANDLE_INFO;
    typedef struct _CLIENT_ID
    {
        HANDLE UniqueProcess;
        HANDLE UniqueThread;
    } CLIENT_ID, *PCLIENT_ID;

    typedef struct _SECTION_IMAGE_INFORMATION
    {
        PVOID TransferAddress; // Entry point
        ULONG ZeroBits;
        SIZE_T MaximumStackSize;
        SIZE_T CommittedStackSize;
        ULONG SubSystemType;
        union
        {
            struct
            {
                USHORT SubSystemMinorVersion;
                USHORT SubSystemMajorVersion;
            } s1;
            ULONG SubSystemVersion;
        } u1;
        union
        {
            struct
            {
                USHORT MajorOperatingSystemVersion;
                USHORT MinorOperatingSystemVersion;
            } s2;
            ULONG OperatingSystemVersion;
        } u2;
        USHORT ImageCharacteristics;
        USHORT DllCharacteristics;
        USHORT Machine;
        BOOLEAN ImageContainsCode;
        union
        {
            UCHAR ImageFlags;
            struct
            {
                UCHAR ComPlusNativeReady : 1;
                UCHAR ComPlusILOnly : 1;
                UCHAR ImageDynamicallyRelocated : 1;
                UCHAR ImageMappedFlat : 1;
                UCHAR BaseBelow4gb : 1;
                UCHAR ComPlusPrefer32bit : 1;
                UCHAR Reserved : 2;
            } s3;
        } u3;
        ULONG LoaderFlags;
        ULONG ImageFileSize;
        ULONG CheckSum;
    } SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;
    typedef struct _PEB_LDR_DATA
    {
        ULONG Length;
        BOOLEAN Initialized;
        HANDLE SsHandle;
        LIST_ENTRY InLoadOrderModuleList;
        LIST_ENTRY InMemoryOrderModuleList;
        LIST_ENTRY InInitializationOrderModuleList;
        PVOID EntryInProgress;
        BOOLEAN ShutdownInProgress;
        HANDLE ShutdownThreadId;
    } PEB_LDR_DATA, *PPEB_LDR_DATA;
    typedef struct _PEB
    {
        BOOLEAN InheritedAddressSpace;
        BOOLEAN ReadImageFileExecOptions;
        BOOLEAN BeingDebugged;
        union
        {
            BOOLEAN BitField;
            struct
            {
                BOOLEAN ImageUsesLargePages : 1;
                BOOLEAN IsProtectedProcess : 1;
                BOOLEAN IsImageDynamicallyRelocated : 1;
                BOOLEAN SkipPatchingUser32Forwarders : 1;
                BOOLEAN IsPackagedProcess : 1;
                BOOLEAN IsAppContainer : 1;
                BOOLEAN IsProtectedProcessLight : 1;
                BOOLEAN IsLongPathAwareProcess : 1;
            } s1;
        } u1;

        HANDLE Mutant;

        PVOID ImageBaseAddress;
        PPEB_LDR_DATA Ldr;
        PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
        PVOID SubSystemData;
        PVOID ProcessHeap;
    } PEB, *PPEB;

    typedef struct _TEB
    {
        NT_TIB NtTib;

        PVOID EnvironmentPointer;
        CLIENT_ID ClientId;
        PVOID ActiveRpcHandle;
        PVOID ThreadLocalStoragePointer;
        PPEB ProcessEnvironmentBlock;
    } TEB, *PTEB;

#define NtCurrentPeb()      (NtCurrentTeb()->ProcessEnvironmentBlock)
#define RtlProcessHeap()    (NtCurrentPeb()->ProcessHeap)

#define PROCESS_CREATE_FLAGS_LARGE_PAGE_SYSTEM_DLL  0x00000020
#define PROCESS_CREATE_FLAGS_PROTECTED_PROCESS      0x00000040
#define PROCESS_CREATE_FLAGS_CREATE_SESSION         0x00000080 // ?
#define PROCESS_CREATE_FLAGS_INHERIT_FROM_PARENT    0x00000100

#define THREAD_CREATE_FLAGS_CREATE_SUSPENDED        0x00000001
#define THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH      0x00000002 // ?
#define THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER      0x00000004
#define THREAD_CREATE_FLAGS_HAS_SECURITY_DESCRIPTOR 0x00000010 // ?
#define THREAD_CREATE_FLAGS_ACCESS_CHECK_IN_TARGET  0x00000020 // ?
#define THREAD_CREATE_FLAGS_INITIAL_THREAD          0x00000080

    NTSYSAPI
        NTSTATUS
        NTAPI
        NtQueryInformationProcess(
            HANDLE ProcessHandle,
            DWORD ProcessInformationClass,
            PVOID ProcessInformation,
            ULONG ProcessInformationLength,
            PULONG ReturnLength
        );

    NTSYSAPI
        NTSTATUS
        NTAPI
        NtResumeThread(
            HANDLE ThreadHandle,
            PULONG SuspendCount OPTIONAL
        );

    
        NTSYSCALLAPI
        NTSTATUS
        NTAPI
        NtRemoveProcessDebug(
            _In_ HANDLE ProcessHandle,
            _In_ HANDLE DebugObjectHandle
        );

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define THIS_MODULE ((HINSTANCE)&__ImageBase)

static DWORD GetParentProcessId() // By Napalm @ NetCore2K
{
    ULONG_PTR pbi[6];
    ULONG ulSize = 0;

    if (NtQueryInformationProcess(GetCurrentProcess(), 0,
        &pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi))
        return (DWORD)pbi[5];

    return -1;
}

void InjectThisDll(HANDLE hProcess)
{
    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW(THIS_MODULE, thisDllPath, _countof(thisDllPath));

    size_t pathSize = (wcslen(thisDllPath) + 1) * sizeof(WCHAR);
    LPVOID pathAddr = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pathAddr, thisDllPath, pathSize, NULL);

    HANDLE loader = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, pathAddr, 0, NULL);
    WaitForSingleObject(loader, INFINITE);
    CloseHandle(loader);
}

// Called by rundll32
int APIENTRY _BootstrapEntry(HWND hwnd, HINSTANCE instance, LPWSTR commandLine, int showFlag)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(commandLine, &argc);

    // Couldn't use std::wstring, heap alloc will cause unknow image path
    WCHAR _NtImagePath[2048]{};
    lstrcpyW(_NtImagePath, L"\\??\\");
    lstrcatW(_NtImagePath, argv[0]);
    // Normalize path slashes
    for (LPWSTR s = _NtImagePath; *s++;)
        if (*s == L'/') *s = '\\';

    // Path to the image file from which the process will be created
    UNICODE_STRING NtImagePath, Params, ImagePath;
    RtlInitUnicodeString(&ImagePath, _NtImagePath + 4);
    RtlInitUnicodeString(&NtImagePath, _NtImagePath);
    RtlInitUnicodeString(&Params, commandLine);

    // Create the process parameters
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters = NULL;
    UNICODE_STRING DesktopInfo = NtCurrentPeb()->ProcessParameters->DesktopInfo;
    NTSTATUS status = RtlCreateProcessParameters(&ProcessParameters, &ImagePath, NULL, NULL, &Params, 0, 0, &DesktopInfo, NULL, NULL);

    if (status != 0)
    {
        char msg[512];
        sprintf_s(msg, "Failed to build process parameters, RtlCreateProcessParameters returns 0x%08X.", status);
        MessageBoxA(NULL, msg, "League Loader bootstraper", MB_OK | MB_ICONWARNING);
        return 1;
    }

    // Initialize the PS_CREATE_INFO structure
    PS_CREATE_INFO CreateInfo;
    memset(&CreateInfo, 0, sizeof(CreateInfo));
    CreateInfo.Size = sizeof(CreateInfo);
    CreateInfo.State = PsCreateInitialState;

    // Skip Image File Execution Options debugger
    CreateInfo.InitState.u1.s1.IFEOSkipDebugger = PsSkipIFEODebugger;

    OBJECT_ATTRIBUTES ObjAttrProcess, ObjAttrThread;
    InitializeObjectAttributes(&ObjAttrProcess, NULL, 0, NULL, NULL);
    InitializeObjectAttributes(&ObjAttrThread, NULL, 0, NULL, NULL);

    PPS_STD_HANDLE_INFO stdHandleInfo = (PPS_STD_HANDLE_INFO)RtlAllocateHeap(RtlProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PS_STD_HANDLE_INFO));
    PCLIENT_ID clientId = (PCLIENT_ID)RtlAllocateHeap(RtlProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PS_ATTRIBUTE));
    PSECTION_IMAGE_INFORMATION SecImgInfo = (PSECTION_IMAGE_INFORMATION)RtlAllocateHeap(RtlProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SECTION_IMAGE_INFORMATION));
    PPS_ATTRIBUTE_LIST AttributeList = (PS_ATTRIBUTE_LIST*)RtlAllocateHeap(RtlProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PS_ATTRIBUTE_LIST));

    PS_ATTRIBUTE *Attribute;
    DWORD numberOfAttributes = 0;

    // Create necessary attributes
    Attribute = &AttributeList->Attributes[numberOfAttributes++];
    Attribute->Attribute = PS_ATTRIBUTE_CLIENT_ID;
    Attribute->Size = sizeof(CLIENT_ID);
    Attribute->ValuePtr = clientId;

    Attribute = &AttributeList->Attributes[numberOfAttributes++];
    Attribute->Attribute = PS_ATTRIBUTE_IMAGE_INFO;
    Attribute->Size = sizeof(SECTION_IMAGE_INFORMATION);
    Attribute->ValuePtr = SecImgInfo;

    Attribute = &AttributeList->Attributes[numberOfAttributes++];
    Attribute->Attribute = PS_ATTRIBUTE_IMAGE_NAME;
    Attribute->Size = NtImagePath.Length;
    Attribute->ValuePtr = NtImagePath.Buffer;

    Attribute = &AttributeList->Attributes[numberOfAttributes++];
    Attribute->Attribute = PS_ATTRIBUTE_STD_HANDLE_INFO;
    Attribute->Size = sizeof(PS_STD_HANDLE_INFO);
    Attribute->ValuePtr = stdHandleInfo;

    //DWORD64 policy = PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_OFF;

    //// Add process mitigation attribute
    //Attribute = &AttributeList->Attributes[numberOfAttributes++];
    //Attribute->Attribute = PS_ATTRIBUTE_MITIGATION_OPTIONS;
    //Attribute->Size = sizeof(DWORD64);
    //Attribute->ValuePtr = &policy;

    // Spoof parent process as debugger's parent to keep process hierarchy
    HANDLE hParent = OpenProcess(PROCESS_ALL_ACCESS, false, GetParentProcessId());
    if (hParent != INVALID_HANDLE_VALUE)
    {
        Attribute = &AttributeList->Attributes[numberOfAttributes++];
        Attribute->Attribute = PS_ATTRIBUTE_PARENT_PROCESS;
        Attribute->Size = sizeof(HANDLE);
        Attribute->ValuePtr = hParent;
    }

    // Set structure length with set of attributes
    AttributeList->TotalLength = FIELD_OFFSET(PS_ATTRIBUTE_LIST, Attributes) + sizeof(PS_ATTRIBUTE) * numberOfAttributes;

    // Create the process
    HANDLE hProcess = NULL, hThread = NULL;
    status = NtCreateUserProcess(&hProcess, &hThread, MAXIMUM_ALLOWED, MAXIMUM_ALLOWED,
        &ObjAttrProcess, &ObjAttrThread, 0, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, ProcessParameters, &CreateInfo, AttributeList);
    
    if (status != 0)
    {
        char msg[512];
        sprintf_s(msg, "Failed to create process, NtCreateUserProcess returns 0x%08X.", status);
        MessageBoxA(NULL, msg, "League Loader bootstraper", MB_OK | MB_ICONWARNING);
    }
    else
    {
        // Inject our module
        InjectThisDll(hProcess);

        // Resume main thread
        NtResumeThread(hThread, NULL);
        WaitForSingleObject(hThread, INFINITE);
    }

    // Clean up
    if (hParent) CloseHandle(hParent);
    RtlFreeHeap(RtlProcessHeap(), 0, AttributeList);
    RtlFreeHeap(RtlProcessHeap(), 0, stdHandleInfo);
    RtlFreeHeap(RtlProcessHeap(), 0, clientId);
    RtlFreeHeap(RtlProcessHeap(), 0, SecImgInfo);
    RtlDestroyProcessParameters(ProcessParameters);

    return status != 0;
}

//NTSTATUS FiCreateProcess(
//    __in PPH_STRING FileName,
//    __in_opt PPH_STRINGREF CommandLine,
//    __in_opt PVOID Environment,
//    __in_opt PPH_STRINGREF CurrentDirectory,
//    __in_opt PPH_CREATE_PROCESS_INFO Information,
//    __in ULONG Flags,
//    __in_opt HANDLE ParentProcessHandle,
//    __out_opt PCLIENT_ID ClientId,
//    __out_opt PHANDLE ProcessHandle,
//    __out_opt PHANDLE ThreadHandle
//    )
//{
//    NTSTATUS status;
//    _NtCreateUserProcess NtCreateUserProcess_I;
//    HANDLE processHandle;
//    HANDLE threadHandle;
//    CLIENT_ID clientId;
//    PRTL_USER_PROCESS_PARAMETERS parameters;
//    PPH_STRING newFileName;
//    UNICODE_STRING fileName;
//    PUNICODE_STRING windowTitle;
//    PUNICODE_STRING desktopInfo;
//
//    NtCreateUserProcess_I = PhGetProcAddress(L"ntdll.dll", "NtCreateUserProcess");
//
//    if (!NtCreateUserProcess_I)
//        return STATUS_NOT_SUPPORTED;
//
//    newFileName = FiFormatFileName(FileName);
//    fileName = newFileName->us;
//
//    if (Information)
//    {
//        windowTitle = (PUNICODE_STRING)Information->WindowTitle;
//        desktopInfo = (PUNICODE_STRING)Information->DesktopInfo;
//    }
//    else
//    {
//        windowTitle = NULL;
//        desktopInfo = NULL;
//    }
//
//    if (!windowTitle)
//        windowTitle = &fileName;
//
//    if (!desktopInfo)
//        desktopInfo = &NtCurrentPeb()->ProcessParameters->DesktopInfo;
//
//    status = RtlCreateProcessParameters(
//        &parameters,
//        &fileName,
//        Information ? (PUNICODE_STRING)Information->DllPath : NULL,
//        (PUNICODE_STRING)CurrentDirectory,
//        CommandLine ? &CommandLine->us : &fileName,
//        Environment,
//        windowTitle,
//        desktopInfo,
//        Information ? (PUNICODE_STRING)Information->ShellInfo : NULL,
//        Information ? (PUNICODE_STRING)Information->RuntimeData : NULL
//        );
//
//    if (NT_SUCCESS(status))
//    {
//        OBJECT_ATTRIBUTES processObjectAttributes;
//        OBJECT_ATTRIBUTES threadObjectAttributes;
//        UCHAR attributeListBuffer[FIELD_OFFSET(PS_ATTRIBUTE_LIST, Attributes) + sizeof(PS_ATTRIBUTE) * 4];
//        PPS_ATTRIBUTE_LIST attributeList;
//        PPS_ATTRIBUTE attribute;
//        ULONG numberOfAttributes;
//        PS_CREATE_INFO createInfo;
//        PS_STD_HANDLE_INFO stdHandleInfo;
//
//        memset(attributeListBuffer, 0, sizeof(attributeListBuffer));
//        attributeList = (PPS_ATTRIBUTE_LIST)attributeListBuffer;
//        numberOfAttributes = 0;
//
//        // Parent process
//        attribute = &attributeList->Attributes[numberOfAttributes++];
//        attribute->Attribute = PS_ATTRIBUTE_PARENT_PROCESS;
//        attribute->Size = sizeof(HANDLE);
//        attribute->ValuePtr = NtCurrentProcess();
//
//        // Image name
//        attribute = &attributeList->Attributes[numberOfAttributes++];
//        attribute->Attribute = PS_ATTRIBUTE_IMAGE_NAME;
//        attribute->Size = fileName.Length;
//        attribute->ValuePtr = fileName.Buffer;
//
//        // Client ID
//        attribute = &attributeList->Attributes[numberOfAttributes++];
//        attribute->Attribute = PS_ATTRIBUTE_CLIENT_ID;
//        attribute->Size = sizeof(CLIENT_ID);
//        attribute->ValuePtr = &clientId;
//
//        // Standard handles
//        attribute = &attributeList->Attributes[numberOfAttributes++];
//        attribute->Attribute = PS_ATTRIBUTE_STD_HANDLE_INFO;
//        attribute->Size = sizeof(PS_STD_HANDLE_INFO);
//        attribute->ValuePtr = &stdHandleInfo;
//
//        attributeList->TotalLength = FIELD_OFFSET(PS_ATTRIBUTE_LIST, Attributes) + sizeof(PS_ATTRIBUTE) * numberOfAttributes;
//
//        if (Flags & PH_CREATE_PROCESS_NEW_CONSOLE)
//        {
//            stdHandleInfo.Flags = 0;
//            stdHandleInfo.StdHandleState = PsNeverDuplicate;
//            stdHandleInfo.StdHandleSubsystemType = 0;
//        }
//        else
//        {
//            // Duplicate standard handles if the image subsystem is Win32 command line.
//            stdHandleInfo.Flags = 0;
//            stdHandleInfo.StdHandleState = PsRequestDuplicate;
//            stdHandleInfo.PseudoHandleMask = PS_STD_INPUT_HANDLE | PS_STD_OUTPUT_HANDLE | PS_STD_ERROR_HANDLE;
//            stdHandleInfo.StdHandleSubsystemType = IMAGE_SUBSYSTEM_WINDOWS_CUI;
//
//            parameters->ConsoleHandle = NtCurrentPeb()->ProcessParameters->ConsoleHandle;
//            parameters->ConsoleFlags = NtCurrentPeb()->ProcessParameters->ConsoleFlags;
//            parameters->StandardInput = NtCurrentPeb()->ProcessParameters->StandardInput;
//            parameters->StandardOutput = NtCurrentPeb()->ProcessParameters->StandardOutput;
//            parameters->StandardError = NtCurrentPeb()->ProcessParameters->StandardError;
//        }
//
//        memset(&createInfo, 0, sizeof(PS_CREATE_INFO));
//        createInfo.Size = sizeof(PS_CREATE_INFO);
//        createInfo.State = PsCreateInitialState;
//        createInfo.InitState.u1.s1.IFEOSkipDebugger = PsSkipIFEODebugger; // ignore Debugger option
//
//        InitializeObjectAttributes(&processObjectAttributes, NULL, 0, NULL, NULL);
//        InitializeObjectAttributes(&threadObjectAttributes, NULL, 0, NULL, NULL);
//
//        parameters = RtlNormalizeProcessParams(parameters);
//        status = NtCreateUserProcess_I(
//            &processHandle,
//            &threadHandle,
//            MAXIMUM_ALLOWED,
//            MAXIMUM_ALLOWED,
//            &processObjectAttributes,
//            &threadObjectAttributes,
//            ((Flags & PH_CREATE_PROCESS_INHERIT_HANDLES) ? PROCESS_CREATE_FLAGS_INHERIT_HANDLES : 0) |
//            ((Flags & PH_CREATE_PROCESS_BREAKAWAY_FROM_JOB) ? PROCESS_CREATE_FLAGS_BREAKAWAY : 0),
//            THREAD_CREATE_FLAGS_CREATE_SUSPENDED,
//            parameters,
//            &createInfo,
//            attributeList
//            );
//        RtlDestroyProcessParameters(parameters);
//    }
//
//    PhDereferenceObject(newFileName);
//
//    if (NT_SUCCESS(status))
//    {
//        if (!(Flags & PH_CREATE_PROCESS_SUSPENDED))
//            NtResumeThread(threadHandle, NULL);
//
//        if (ClientId)
//            *ClientId = clientId;
//
//        if (ProcessHandle)
//            *ProcessHandle = processHandle;
//        else
//            NtClose(processHandle);
//
//        if (ThreadHandle)
//            *ThreadHandle = threadHandle;
//        else
//            NtClose(threadHandle);
//    }
//
//    return status;
//}