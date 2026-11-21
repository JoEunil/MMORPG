using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Mmorpg.Client
{
    // Reads the same named, strictly typed CSV records as CoreLib/Data/CsvReader.
    internal sealed class ContentCsvReader : IDisposable
    {
        private readonly string path;
        private readonly StringReader input;
        private readonly Dictionary<string, int> columns = new Dictionary<string, int>(StringComparer.Ordinal);
        private string[] row;
        private int line = 1;
        private int rowLine = 1;

        internal ContentCsvReader(string path, params string[] expectedColumns)
        {
            this.path = path;
            string text = File.ReadAllText(path, new UTF8Encoding(false, true));
            input = new StringReader(text.Length > 0 && text[0] == '\uFEFF' ? text.Substring(1) : text);
            var header = ReadRecord();
            if (header == null) Fail("", "missing header");
            for (int i = 0; i < header.Length; i++)
            {
                if (header[i].Length == 0 || columns.ContainsKey(header[i])) Fail(header[i], "empty or duplicate column name");
                columns.Add(header[i], i);
            }
            foreach (string column in expectedColumns)
                if (!columns.ContainsKey(column)) Fail(column, "missing required column");
            if (columns.Count != expectedColumns.Length) Fail("", "unexpected column in header");
        }

        internal bool Read()
        {
            do { row = ReadRecord(); } while (row != null && row.Length == 0);
            if (row == null) return false;
            if (row.Length != columns.Count) Fail("", "field count does not match header");
            return true;
        }

        internal string Text(string column) => row[columns[column]];
        internal string Name(string column)
        {
            string value = Text(column);
            if (value.Length == 0 || value.IndexOf('\0') >= 0) Fail(column, "name must be nonempty and cannot contain a null character");
            return value;
        }

        internal ulong Unsigned(string column, ulong maximum = uint.MaxValue)
        {
            if (!ulong.TryParse(Text(column), NumberStyles.None, CultureInfo.InvariantCulture, out ulong value) || value > maximum)
                Fail(column, "expected an unsigned integer within the field's range");
            return value;
        }

        internal uint UInt(string column, uint maximum = uint.MaxValue) => (uint)Unsigned(column, maximum);
        internal int Index(string column, int maximum = ushort.MaxValue) => (int)Unsigned(column, (ulong)maximum);
        internal float Float(string column)
        {
            const NumberStyles style = NumberStyles.AllowLeadingSign | NumberStyles.AllowDecimalPoint | NumberStyles.AllowExponent;
            if (!float.TryParse(Text(column), style, CultureInfo.InvariantCulture, out float value) || Text(column).StartsWith("+", StringComparison.Ordinal) || float.IsNaN(value) || float.IsInfinity(value))
                Fail(column, "expected a finite floating-point value within the field's range");
            return value;
        }

        internal void Require(bool condition, string column, string message)
        {
            if (!condition) Fail(column, message);
        }

        internal void Fail(string column, string message) => throw new InvalidDataException(
            path + ":" + rowLine + (column.Length == 0 ? "" : " [" + column + "]") + ": " + message);

        private string[] ReadRecord()
        {
            var fields = new List<string>();
            var field = new StringBuilder();
            bool quoted = false, afterQuote = false, consumed = false;
            rowLine = line;
            while (true)
            {
                int next = input.Read();
                if (next == -1)
                {
                    if (quoted) Fail("", "unterminated quoted field");
                    if (!consumed) return null;
                    fields.Add(field.ToString());
                    return fields.ToArray();
                }
                char character = (char)next;
                bool atStart = !consumed;
                consumed = true;
                if (quoted)
                {
                    if (character == '"')
                    {
                        if (input.Peek() == '"') { input.Read(); field.Append('"'); }
                        else { quoted = false; afterQuote = true; }
                    }
                    else
                    {
                        field.Append(character);
                        if (character == '\n' || character == '\r' && input.Peek() != '\n') line++;
                    }
                }
                else if (character == ',')
                {
                    fields.Add(field.ToString()); field.Clear(); afterQuote = false;
                }
                else if (character == '\r' || character == '\n')
                {
                    if (character == '\r' && input.Peek() == '\n') input.Read();
                    line++;
                    if (atStart) return Array.Empty<string>();
                    fields.Add(field.ToString());
                    return fields.ToArray();
                }
                else if (character == '"' && field.Length == 0 && !afterQuote) quoted = true;
                else
                {
                    if (afterQuote || character == '"') Fail("", "invalid quote in CSV field");
                    field.Append(character);
                }
            }
        }

        public void Dispose() => input.Dispose();
    }
}
