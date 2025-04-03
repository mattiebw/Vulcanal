namespace Preprocessor
{
    internal interface IProcessor
    {
        void ImportFile(string filename, string outputFilename);
        List<(string, int)> GetValidFileExtensions();
    }
}
