namespace Preprocessor;

public static class Utility
{
    public static string TryGetExecutablePath(string filename)
    {
        filename = Environment.ExpandEnvironmentVariables(filename);
        if (File.Exists(filename))
            return filename;
        
        string envPath = Environment.GetEnvironmentVariable("PATH")!;
        envPath = Environment.ExpandEnvironmentVariables(envPath);
        var pathSeperator = ';';
        if (Environment.OSVersion.Platform == PlatformID.Unix)
            pathSeperator = ':';
        var paths = envPath.Split(pathSeperator);

        foreach (var path in paths)
        {
            string fullPath = Path.Combine(path, filename);
            if (File.Exists(fullPath))
                return Path.GetFullPath(fullPath);
        }

        return "";
    }
}