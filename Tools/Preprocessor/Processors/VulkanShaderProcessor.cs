using System.Diagnostics;

namespace Preprocessor.Processors;

public class VulkanShaderProcessor : IProcessor
{
    private string _glslValidatorPath = "";
    
    public VulkanShaderProcessor()
    {
        string filename = "glslangValidator";
        if (Environment.OSVersion.Platform == PlatformID.Win32NT)
            filename = "glslangValidator.exe";

        _glslValidatorPath = Utility.TryGetExecutablePath(filename);
        if (string.IsNullOrEmpty(_glslValidatorPath))
        {
            var vulkanSdk = Environment.GetEnvironmentVariable("VULKAN_SDK");
            if (string.IsNullOrEmpty(vulkanSdk))
            {
                Log.Error("Couldn't find glslangValidator; not directly in path, and VULKAN_SDK is not set.");
                return;
            }
            
            _glslValidatorPath = Path.Combine(vulkanSdk!, "/Bin/", filename);
            if (!File.Exists(_glslValidatorPath))
            {
                Log.Error($"Failed to find glslangValidator.");
                _glslValidatorPath = "";
            }
        }
    }
    
    public void ImportFile(string filename, string outputFilename)
    {
        if (string.IsNullOrEmpty(_glslValidatorPath))
            return;
        
        var modifiedOutputPath = Path.ChangeExtension(outputFilename,
            Program.PreprocessorSettings.GetProcessorSetting("CompiledShaderExtension", ".spv"));
        Directory.CreateDirectory(Path.GetDirectoryName(modifiedOutputPath)!);
                
        var process = Process.Start(_glslValidatorPath, [ "-V", filename, "-o", modifiedOutputPath ]);
        process.WaitForExit();

        if (process.ExitCode != 0)
            throw new Exception($"Failed to compile shader: {filename}");
    }

    public List<(string, int)> GetValidFileExtensions()
    {
        return [(".glsl", 1), (".hlsl", 1), (".comp", 1)];
    }
}
