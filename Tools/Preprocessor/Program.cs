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
    public List<string> IgnoredFilePatterns { get; set; } = [];
    public Dictionary<string, List<LibraryToCopy>> LibraryCopies { get; set; } = [];
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
    private const string FileLastProcessedFileName = "AssetsLastProcessed.txt";

    // Map of file extensions -> the processor and its current priority.
    private static readonly Dictionary<string, (IProcessor, int)> AssetProcessors = [];

    // Current highest priority global processor (processor with accepted file extension "*")
    private static (IProcessor?, int) _globalProcessor = (null, 0);

    // Preprocessor settings! Contains library copies, ignored file patterns, and processor settings (such as file extensions).
    public static PreprocessorSettings PreprocessorSettings = new PreprocessorSettings();

    private static void Main(string[] args)
    {
        if (args.Length != 3)
        {
            Log.Error(
                $"Give the content source directory as the first argument, and the content output directory as the second, and the platform as the third. ({args.Length} args given)");
            Console.ResetColor();
            Environment.ExitCode = -1;
            return;
        }

        var contentDir = Path.GetFullPath(args[0]);
        var outputDir = Path.GetFullPath(args[1]);
        
        if (!Directory.Exists(contentDir))
        {
            Log.Error($"Content directory {contentDir} does not exist.");
            Environment.ExitCode = -1;
            return;
        }
        
        if (!Directory.Exists(outputDir))
        {
            Log.Warning($"Output directory {outputDir} does not exist; creating it.");
            Directory.CreateDirectory(outputDir);
        }
        
        Log.Info(
            $"Running Preprocessor, with:\n\tContent directory: {contentDir}\n\tOutput directory: {outputDir}\n\tPlatform: {args[2]}");

        LoadSettings(contentDir);
        LoadLastProcessedTimes();
        LoadProcessors();
        DoLibCopies(contentDir, outputDir, args[2]);
        DoAssetProcessing(contentDir, outputDir);
        SaveLastProcessedTimes();

        Log.ResetLog();
    }

    private static void LoadSettings(string sourceDirectory)
    {
        var jsonSettings = new JsonSerializerOptions
        {
            AllowTrailingCommas = true,
            ReadCommentHandling = JsonCommentHandling.Skip,
            WriteIndented = true
        };

        var settingsFile = Path.Join(sourceDirectory, "preprocessor_settings.json");
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

        if (!File.Exists(FileLastProcessedFileName))
        {
            Log.Warning(
                $"No asset last processed times found (checked for {FileLastProcessedFileName} in {Environment.CurrentDirectory})");
            return;
        }

        var fileData = File.ReadAllText(FileLastProcessedFileName);
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

            FileLastProcessed.Add(parts[0], DateTime.Parse(parts[1]));
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

    private static void DoLibCopies(string sourceDirectory, string outputDirectory, string platform)
    {
        if (!PreprocessorSettings.LibraryCopies.TryGetValue(platform, out var copies))
            return;

        foreach (var libCopy in copies)
        {
            string input = Path.Join(sourceDirectory, libCopy.RelativePath);
            if (!File.Exists(input))
            {
                Log.Warning($"Library copy file {input} does not exist; skipping.");
                continue;
            }

            string output = Path.Join(outputDirectory, libCopy.OutputRelativePath, Path.GetFileName(input));
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

    private static void DoAssetProcessing(string sourceDirectory, string outputDirectory)
    {
        if (!Directory.Exists(sourceDirectory))
        {
            Log.Error($"Provided source directory \"{sourceDirectory}\" does not exist.");
            return;
        }

        if (!Directory.Exists(outputDirectory))
        {
            Log.Warning($"Provided output directory \"{outputDirectory}\" does not exist; creating it.");
            Directory.CreateDirectory(outputDirectory);
        }

        var files = Directory.GetFiles(sourceDirectory, "*", SearchOption.AllDirectories);
        foreach (var file in files)
        {
            if (file.EndsWith("preprocessor_settings.json"))
                continue;

            var lastWrite = File.GetLastWriteTime(file);
            if (FileLastProcessed.TryGetValue(file, out var lastProcessedTime))
            {
                if (lastProcessedTime.AddSeconds(1) >=
                    lastWrite) // MW @hack: We add a second to the last write time to account for the fact that the lastWrite time is more accurate than the last processed time we loaded.
                    continue;
            }

            var ext = Path.GetExtension(file);
            var outputFilename = Path.Join(outputDirectory, Path.GetRelativePath(sourceDirectory, file));

            if (AssetProcessors.ContainsKey(ext))
            {
                try
                {
                    AssetProcessors[ext].Item1.ImportFile(file, outputFilename);
                }
                catch (Exception e)
                {
                    Log.Error(
                        $"Failed to import file {Path.GetRelativePath(sourceDirectory, file)} with processor {AssetProcessors[ext].Item1.GetType().Name}: {e.Message}");
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
                        $"Failed to import file {Path.GetRelativePath(sourceDirectory, file)} with processor {_globalProcessor.Item1.GetType().Name}: {e.Message}");
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
            output.AppendLine($"{file};{date.ToString("u")}");
        }

        File.WriteAllText(FileLastProcessedFileName, output.ToString());
        Log.Info("Saved asset last processed times.");
    }
}
