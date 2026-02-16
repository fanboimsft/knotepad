#ifndef RTFHANDLER_H
#define RTFHANDLER_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QVector>

/**
 * RTF reader/writer for QTextDocument.
 *
 * Supports: bold, italic, underline, strikethrough, font family, font size,
 * text color, bullet lists, and paragraph breaks.
 */
class RtfHandler {
public:
  // Read RTF data and populate the given QTextDocument
  static bool readRtf(const QByteArray &rtfData, QTextDocument *doc);

  // Write the QTextDocument content as RTF
  static QByteArray writeRtf(const QTextDocument *doc);

private:
  // Internal structures for the RTF parser
  struct FontEntry {
    int id = 0;
    QString name;
  };

  struct ColorEntry {
    int red = 0;
    int green = 0;
    int blue = 0;
  };

  struct CharState {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    int fontIndex = 0;     // index into font table
    int fontSize = 24;     // in half-points (24 = 12pt)
    int colorIndex = 0;    // 0 = default/auto
  };

  struct ParaState {
    bool inList = false;
    int leftIndent = 0;
    int firstLineIndent = 0;
  };

  struct State {
    CharState charState;
    ParaState paraState;
    bool skipDestination = false;
  };

  // RTF tokenizer
  enum class TokenType { GroupStart, GroupEnd, ControlWord, Text, HexChar };

  struct Token {
    TokenType type;
    QString word;      // control word name (without backslash)
    int parameter = 0; // numeric parameter (-1 if absent)
    bool hasParam = false;
    QString text;      // for Text tokens
    int hexValue = 0;  // for HexChar tokens
  };

  static QVector<Token> tokenize(const QByteArray &data);
};

#endif // RTFHANDLER_H
