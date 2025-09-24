using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;

namespace VulkEase
{
    /// <summary>
    /// Dynamic library loader for VulkEase native libraries
    /// Automatically handles x86/x64 platform detection and loading
    /// </summary>
    public class VulkEaseDllLoader
    {
        private IntPtr myDllPtr { get; set; }

        /// <summary>
        /// Creates a new DLL loader and loads the VulkEase library
        /// </summary>
        /// <param name="dllName">Name of the DLL to load (e.g. "vulkease")</param>
        public VulkEaseDllLoader(string dllName)
        {
            LoadSystemDll(dllName);
        }

        /// <summary>
        /// Loads a dll into process memory.
        /// </summary>
        /// <param name="lpFileName">Filename to load.</param>
        /// <returns>Pointer to the loaded library.</returns>
        /// <remarks>
        /// This method is used to load a dll into memory, before calling any of its DllImported methods.
        /// 
        /// This is done to allow loading an x86 version of a dll for an x86 process, or an x64 version of it
        /// for an x64 process.
        /// </remarks>
        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
        public static extern IntPtr LoadLibrary([MarshalAs(UnmanagedType.LPStr)] string lpFileName);

        /// <summary>
        /// Frees a previously loaded dll, from process memory.
        /// </summary>
        /// <param name="hModule">Pointer to the previously loaded library (This pointer comes from a call to LoadLibrary).</param>
        /// <returns>Returns true if the library was successfully freed.</returns>
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool FreeLibrary(IntPtr hModule);

        /// <summary>
        /// This method is used to load the dll into memory, before calling any of its DllImported methods.
        /// 
        /// This is done to allow loading an x86 or x64 version of the dll depending on the process
        /// </summary>
        private IntPtr LoadDll(string dllName)
        {
            if (myDllPtr == IntPtr.Zero)
            {
                // Retrieve the folder of the executing assembly
                string executingAssemblyFolder = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);

                string subfolder;

                if (Environment.Is64BitProcess)
                    subfolder = "x64";
                else
                    subfolder = "x32";

                string filename = Path.Combine(executingAssemblyFolder, subfolder, dllName + ".dll");

                // Check that the dll file exists
                bool exists = File.Exists(filename);
                if (!exists)
                    throw new DllNotFoundException("Unable to load the file \"" + filename + "\", the file wasn't found.");

                myDllPtr = LoadLibrary(filename);
                if (myDllPtr == IntPtr.Zero)
                {
                    int win32Error = Marshal.GetLastWin32Error();
                    throw new Win32Exception(win32Error, "Unable to load the file \"" + filename + "\", LoadLibrary reported error code: " + win32Error + ".");
                }
            }

            return myDllPtr;
        }

        /// <summary>
        /// Loads a system DLL (searches system paths)
        /// </summary>
        /// <param name="dllName">Name of the DLL to load</param>
        /// <returns>Handle to the loaded library</returns>
        private IntPtr LoadSystemDll(string dllName)
        {
            // First try to load from system paths
            myDllPtr = LoadLibrary(dllName + ".dll");
            
            // If that fails, try loading from the application directory structure
            if (myDllPtr == IntPtr.Zero)
            {
                myDllPtr = LoadDll(dllName);
            }

            return myDllPtr;
        }

        /// <summary>
        /// Frees previously loaded dll, from process memory.
        /// </summary>
        /// <returns>IntPtr.Zero if successful</returns>
        public IntPtr UnloadDll()
        {
            if (myDllPtr != IntPtr.Zero)
            {
                bool success = FreeLibrary(myDllPtr);
                if (success)
                    myDllPtr = IntPtr.Zero;
            }

            return myDllPtr;
        }

        /// <summary>
        /// Gets whether the DLL is currently loaded
        /// </summary>
        public bool IsLoaded => myDllPtr != IntPtr.Zero;

        /// <summary>
        /// Gets the handle to the loaded DLL
        /// </summary>
        public IntPtr Handle => myDllPtr;

        /// <summary>
        /// Finalizer to ensure proper cleanup
        /// </summary>
        ~VulkEaseDllLoader()
        {
            UnloadDll();
        }

        /// <summary>
        /// Dispose pattern implementation
        /// </summary>
        public void Dispose()
        {
            UnloadDll();
            GC.SuppressFinalize(this);
        }
    }

    /// <summary>
    /// Static helper class for VulkEase library management
    /// Automatically loads the VulkEase library when first accessed
    /// </summary>
    public static class VulkEaseLibrary
    {
        private static VulkEaseDllLoader _loader;
        private static readonly object _loaderLock = new object();

        /// <summary>
        /// Ensures the VulkEase library is loaded
        /// </summary>
        public static void EnsureLoaded()
        {
            if (_loader == null)
            {
                lock (_loaderLock)
                {
                    if (_loader == null)
                    {
                        _loader = new VulkEaseDllLoader("vulkease");
                    }
                }
            }
        }

        /// <summary>
        /// Manually unloads the VulkEase library
        /// </summary>
        public static void Unload()
        {
            lock (_loaderLock)
            {
                _loader?.Dispose();
                _loader = null;
            }
        }

        /// <summary>
        /// Gets whether the VulkEase library is currently loaded
        /// </summary>
        public static bool IsLoaded
        {
            get
            {
                lock (_loaderLock)
                {
                    return _loader?.IsLoaded ?? false;
                }
            }
        }

        /// <summary>
        /// Static constructor to ensure library is loaded when VulkEase functions are first called
        /// </summary>
        static VulkEaseLibrary()
        {
            EnsureLoaded();
        }
    }
}