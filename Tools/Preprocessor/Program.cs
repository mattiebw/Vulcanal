using System.Globalization;
using System.Reflection;
using System.Text;
using System.Text.Json;

namespace Preprocessor;

[Serializable]
internal struct LibraryToCopy
{
    public string RelativePath { get; set; } = "";
    public string OutputRelativePath { get; set; } = "";

    public LibraryToCopy(string relativePath, string outputRelativePath = "")
    {
        RelativePath = relativePath;
        OutputRelativePath = outputRelativePath;
    }
}

[Serializable]
internal struct PreprocessorSettings
{
    /// <summary>
    /// If a file matches this pattern, we ignore it.
    /// MW @todo: Implement
    /// </summary>
    public List<string> IgnoredFilePatterns { get; set; } = [];

    /// <summary>
    /// Paths to add to the system "PATH" variable for all spawned processes, if they exist.
    /// Can be useful for limited systems where you can't edit the PATH for the entire system,
    /// but you still want to add a path for the preprocessor to use.
    /// </summary>
    public List<string> SpeculativePathAdditions { get; set; } = [];

    /// <summary>
    /// Library files to copy to the build directory.
    /// </summary>
    public Dictionary<string, List<LibraryToCopy>> LibraryCopies { get; set; } = [];

    /// <summary>
    /// Other misc settings to be set for the different processors.
    /// </summary>
    public Dictionary<string, string> ProcessorSettings { get; set; } = [];

    public PreprocessorSettings()
    {
    }

    public string GetProcessorSetting(string setting, string defaultValue = "") =>
        ProcessorSettings.TryGetValue(setting, out var value) ? value : defaultValue;
}

internal static class Program
{
    private static readonly Dictionary<string, DateTime> FileLastProcessed = [];
    
    // File to save the last processed times to. Updated at startup to include the configuration.
    private static string _fileLastProcessedFileName = "";

    // Map of file extensions -> the processor and its current priority.
    private static readonly Dictionary<string, (IProcessor, int)> AssetProcessors = [];

    // Current highest priority global processor (processor with accepted file extension "*")
    private static (IProcessor?, int) _globalProcessor = (null, 0);

    // Preprocessor settings! Contains library copies, ignored file patterns, and processor settings (such as file extensions).
    public static PreprocessorSettings PreprocessorSettings = new();
    public static string ContentDir = "", OutputDir = "";
    public static string Platform = "";
    public static string Configuration = "";

    private static void Main(string[] args)
    {
        if (args.Length != 4)
        {
            Log.Error(
                $"Give the content source directory as the first argument, the content output directory as " +
                $"the second, the platform as the third, and the configuration as the fourth. ({args.Length} args given)");
            Console.ResetColor();
            Environment.ExitCode = -1;
            return;
        }

        ContentDir = Path.GetFullPath(args[0]);
        OutputDir = Path.GetFullPath(args[1]);
        Platform = args[2];
        Configuration = args[3];
        
        // We update the file name to include the platform and configuration - this means that assets will be 
        // reprocessed for every platform/configuration combination.
        _fileLastProcessedFileName = $"AssetLastProcessedTimes_{Platform}_{Configuration}.txt";

        if (!Directory.Exists(ContentDir))
        {
            Log.Error($"Content directory {ContentDir} does not exist.");
            Environment.ExitCode = -1;
            return;
        }

        if (!Directory.Exists(OutputDir))
        {
            Log.Warning($"Output directory {OutputDir} does not exist; creating it.");
            Directory.CreateDirectory(OutputDir);
        }

        Log.Info(
            $"Running Preprocessor, with:\n\tContent directory: {ContentDir}\n\tOutput directory: {OutputDir}\n\tPlatform: {args[2]}");

        LoadSettings();

        // Add our speculative path additions.
        if (Environment.OSVersion.Platform == PlatformID.Win32NT)
        {
            StringBuilder newPath = new StringBuilder(Environment.GetEnvironmentVariable("PATH"));
            foreach (string path in PreprocessorSettings.SpeculativePathAdditions)
            {
                if (Directory.Exists(path))
                    newPath.Append(';').Append(path);
            }

            Environment.SetEnvironmentVariable("PATH", newPath.ToString(), EnvironmentVariableTarget.Process);
        }

        LoadLastProcessedTimes();
        LoadProcessors();
        DoLibCopies();
        DoAssetProcessing();
        SaveLastProcessedTimes();

        Log.ResetLog();
    }

    private static void LoadSettings()
    {
        var jsonSettings = new JsonSerializerOptions
        {
            AllowTrailingCommas = true,
            ReadCommentHandling = JsonCommentHandling.Skip,
            WriteIndented = true
        };

        var settingsFile = Path.Join(ContentDir, "preprocessor_settings.json");
        if (!File.Exists(settingsFile))
        {
            Log.Warning(
                $"No preprocessor settings file found (checked for {settingsFile}); using defaults, writing new settings file.");
            File.WriteAllText(settingsFile, JsonSerializer.Serialize(PreprocessorSettings, jsonSettings));
            return;
        }

        var fileData = File.ReadAllText(settingsFile);
        try
        {
            PreprocessorSettings = JsonSerializer.Deserialize<PreprocessorSettings>(fileData, jsonSettings);
        }
        catch (Exception e)
        {
            Log.Error($"Failed to load preprocessor settings file: {e.Message}");
            return;
        }

        Log.Info($"Loaded preprocessor settings: {settingsFile}");
    }

    private static void LoadLastProcessedTimes()
    {
#if DEBUG
        Log.Warning("Skipping loading last processed times in debug mode - we will import every single asset.");
        return;
#endif

        if (!File.Exists(_fileLastProcessedFileName))
        {
            Log.Warning(
                $"No asset last processed times found (checked for {_fileLastProcessedFileName} in {Environment.CurrentDirectory})");
            return;
        }

        var fileData = File.ReadAllText(_fileLastProcessedFileName);
        string[] lines = fileData.Split("\n");

        foreach (var line in lines)
        {
            if (string.IsNullOrWhiteSpace(line))
                continue;

            var trimmed = line.Trim();
            string[] parts = line.Split(';');
            if (parts.Length != 2)
            {
                Log.Error($"Invalid last asset processing time line: {trimmed}");
                continue;
            }

            FileLastProcessed.Add(parts[0], DateTime.Parse(parts[1], CultureInfo.InvariantCulture).ToUniversalTime());
        }

        Log.Info($"Loaded {FileLastProcessed.Count} asset last processed times.");
    }

    private static void LoadProcessors()
    {
        // MW @todo: We could definitely do this with codegen instead of runtime reflection.
        Assembly.GetExecutingAssembly().GetTypes()
            .Where(t => t.IsClass && !t.IsAbstract && t.GetInterfaces().Contains(typeof(IProcessor)))
            .ToList()
            .ForEach(t =>
            {
                var processor = (IProcessor)Activator.CreateInstance(t)!;
                foreach (var (ext, priority) in processor.GetValidFileExtensions())
                {
                    if (ext == "*")
                    {
                        if (_globalProcessor.Item2 < priority)
                        {
                            _globalProcessor = (processor, priority);
                        }
                    }
                    else
                    {
                        if (AssetProcessors.ContainsKey(ext))
                        {
                            if (AssetProcessors[ext].Item2 < priority)
                            {
                                AssetProcessors[ext] = (processor, priority);
                            }
                        }
                        else
                        {
                            AssetProcessors.Add(ext, (processor, priority));
                        }
                    }
                }
            });
    }

    private static void DoLibCopies()
    {
        if (!PreprocessorSettings.LibraryCopies.TryGetValue(Platform, out var copies))
            return;

        foreach (var libCopy in copies)
        {
            string input = Path.Join(ContentDir, libCopy.RelativePath);
            if (!File.Exists(input))
            {
                Log.Warning($"Library copy file {input} does not exist; skipping.");
                continue;
            }

            string output = Path.Join(OutputDir, libCopy.OutputRelativePath, Path.GetFileName(input));
            if (File.Exists(output))
            {
                Log.Trace($"Library file \"{input}\" already exists at \"{output}\"; skipping.");
                continue;
            }

            try
            {
                Log.Trace("Copying library file \"{input}\" to \"{output}\"");
                File.Copy(input, output);
            }
            catch (Exception e)
            {
                Log.Error($"Failed to copy library file \"{input}\" to \"{output}\": {e.Message}");
            }
        }
    }

    private static void DoAssetProcessing()
    {
        var files = Directory.GetFiles(ContentDir, "*", SearchOption.AllDirectories);
        foreach (var file in files)
        {
            if (file.EndsWith("preprocessor_settings.json"))
                continue;

            var lastWrite = File.GetLastWriteTime(file);
            lastWrite = lastWrite
                .ToUniversalTime(); // Prevent issues with local time zones - without this, the time would change
            // every time we ran the preprocessor; in BST for example, which is +1 hour from
            // UTC, the last modified time would jump 1 hour every time we ran the preprocessor.
            if (FileLastProcessed.TryGetValue(file, out var lastProcessedTime))
            {
                if (lastProcessedTime.AddSeconds(1) >=
                    lastWrite) // MW @hack: We add a second to the last write time to account for the fact that the lastWrite time is more accurate than the last processed time we loaded.
                    continue;
            }

            var ext = Path.GetExtension(file);
            var outputFilename = Path.Join(OutputDir, Path.GetRelativePath(ContentDir, file));

            if (AssetProcessors.ContainsKey(ext))
            {
                try
                {
                    AssetProcessors[ext].Item1.ImportFile(file, outputFilename);
                }
                catch (Exception e)
                {
                    Log.Error(
                        $"Failed to import file {Path.GetRelativePath(ContentDir, file)} with processor {AssetProcessors[ext].Item1.GetType().Name}: {e.Message}");
                    continue;
                }
            }
            else if (_globalProcessor.Item1 is not null)
            {
                try
                {
                    _globalProcessor.Item1.ImportFile(file, outputFilename);
                }
                catch (Exception e)
                {
                    Log.Error(
                        $"Failed to import file {Path.GetRelativePath(ContentDir, file)} with processor {_globalProcessor.Item1.GetType().Name}: {e.Message}");
                    continue;
                }
            }
            else
            {
                Log.Warning($"No processor found for file: {file}");
                continue;
            }

            if (!FileLastProcessed.TryAdd(file, lastWrite))
            {
                // Technically TryAdd could fail for other reasons than the key already existing, but we'll just assume for now that it won't...
                FileLastProcessed[file] = lastWrite;
            }
        }
    }

    /// <summary>
    /// Save the last time each processed file was modified, so we can skip reprocessing it next time if it hasn't changed.
    /// </summary>
    private static void SaveLastProcessedTimes()
    {
        StringBuilder output = new();
        foreach (var (file, date) in FileLastProcessed)
        {
            // Our format for this file is the full file name, a semicolon, and the last processed time in UTC format.
            output.AppendLine($"{file};{date.ToString("u", CultureInfo.InvariantCulture)}");
        }

        File.WriteAllText(_fileLastProcessedFileName, output.ToString());
        Log.Info("Saved asset last processed times.");
    }
}
