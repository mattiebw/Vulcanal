namespace Preprocessor.Processors;

internal class SimpleCopyProcessor : IProcessor
{
    public List<(string, int)> GetValidFileExtensions()
    {
        return [("*", 1)];
    }

    public void ImportFile(string filename, string outputFilename)
    {
        if (File.Exists(outputFilename))
            File.Delete(outputFilename);

        Directory.CreateDirectory(Path.GetDirectoryName(outputFilename)!);

        File.Copy(filename, outputFilename);

        Log.Trace($"{filename} -> {outputFilename}");
    }
}