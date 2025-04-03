namespace Preprocessor
{
    internal static class Log
    {
        public static void Trace(string message)
        {
#if DEBUG
            Console.ForegroundColor = ConsoleColor.White;
            Console.WriteLine(message);
#endif
        }

        public static void Info(string message)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine(message);
        }

        public static void Warning(string message)
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine(message);
        }

        public static void Error(string message)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine(message);
        }

        public static void ResetLog()
        {
            Console.ResetColor();
        }
    }
}
