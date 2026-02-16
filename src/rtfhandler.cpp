#include "rtfhandler.h"

#include <QFont>
#include <QStack>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextListFormat>

// ============================================================================
// Tokenizer
// ============================================================================

QVector<RtfHandler::Token> RtfHandler::tokenize(const QByteArray &data) {
  QVector<Token> tokens;
  int i = 0;
  int len = data.size();

  while (i < len) {
    char ch = data[i];

    if (ch == '{') {
      Token t;
      t.type = TokenType::GroupStart;
      tokens.append(t);
      i++;
    } else if (ch == '}') {
      Token t;
      t.type = TokenType::GroupEnd;
      tokens.append(t);
      i++;
    } else if (ch == '\\') {
      i++; // skip backslash
      if (i >= len)
        break;

      ch = data[i];

      // Hex character: \'xx
      if (ch == '\'') {
        i++;
        if (i + 1 < len) {
          Token t;
          t.type = TokenType::HexChar;
          QByteArray hexStr = data.mid(i, 2);
          bool ok;
          t.hexValue = hexStr.toInt(&ok, 16);
          tokens.append(t);
          i += 2;
        }
        continue;
      }

      // Control symbol (non-letter after backslash)
      if (!QChar::isLetter(ch)) {
        // Special: \~ = non-breaking space, \- = optional hyphen, \_ = non-breaking hyphen
        // \n and \r are paragraph breaks like \par
        if (ch == '\n' || ch == '\r') {
          Token t;
          t.type = TokenType::ControlWord;
          t.word = QStringLiteral("par");
          tokens.append(t);
        } else if (ch == '~') {
          Token t;
          t.type = TokenType::Text;
          t.text = QChar(0xA0); // non-breaking space
          tokens.append(t);
        } else if (ch == '*') {
          // \* marks a "destination" group that can be skipped if unknown
          Token t;
          t.type = TokenType::ControlWord;
          t.word = QStringLiteral("*");
          tokens.append(t);
        } else {
          // Other control symbols: output the character itself
          Token t;
          t.type = TokenType::Text;
          t.text = QChar::fromLatin1(ch);
          tokens.append(t);
        }
        i++;
        continue;
      }

      // Control word: letters followed by optional digits, terminated by space or non-alpha
      QString word;
      while (i < len && QChar::isLetter(data[i])) {
        word += QChar::fromLatin1(data[i]);
        i++;
      }

      Token t;
      t.type = TokenType::ControlWord;
      t.word = word;

      // Optional numeric parameter (including negative numbers)
      if (i < len && (data[i] == '-' || QChar::isDigit(data[i]))) {
        QString numStr;
        if (data[i] == '-') {
          numStr += QLatin1Char('-');
          i++;
        }
        while (i < len && QChar::isDigit(data[i])) {
          numStr += QChar::fromLatin1(data[i]);
          i++;
        }
        t.hasParam = true;
        t.parameter = numStr.toInt();
      }

      // A space after a control word is a delimiter and is consumed
      if (i < len && data[i] == ' ') {
        i++;
      }

      tokens.append(t);
    } else if (ch == '\r' || ch == '\n') {
      // Bare CR/LF outside control words are ignored in RTF
      i++;
    } else {
      // Plain text - collect until we hit a special character
      QString text;
      while (i < len && data[i] != '{' && data[i] != '}' && data[i] != '\\' &&
             data[i] != '\r' && data[i] != '\n') {
        text += QChar::fromLatin1(data[i]);
        i++;
      }
      if (!text.isEmpty()) {
        Token t;
        t.type = TokenType::Text;
        t.text = text;
        tokens.append(t);
      }
    }
  }

  return tokens;
}

// ============================================================================
// RTF Reader
// ============================================================================

bool RtfHandler::readRtf(const QByteArray &rtfData, QTextDocument *doc) {
  if (!doc)
    return false;

  QVector<Token> tokens = tokenize(rtfData);
  if (tokens.isEmpty())
    return false;

  // Font table and color table
  QVector<FontEntry> fontTable;
  QVector<ColorEntry> colorTable;

  // State stack for group nesting
  QStack<State> stateStack;
  State currentState;

  // Parsing modes
  enum class ParseMode { Normal, FontTable, ColorTable, SkipGroup };
  QStack<ParseMode> modeStack;
  modeStack.push(ParseMode::Normal);

  // For font table parsing
  FontEntry currentFont;
  QString fontNameAccum;

  // For color table parsing
  ColorEntry currentColor;
  bool colorHasComponent = false;

  // Track group depth for skip mode
  int skipDepth = 0;

  // Track if we've seen the \rtf1 header
  bool seenRtfHeader = false;

  // Build the document
  doc->clear();
  QTextCursor cursor(doc);

  // We need to track if this is the first paragraph (to avoid leading empty line)
  bool firstParagraph = true;

  // Track pending list state — will be applied at \par
  bool pendingListItem = false;

  // Track the default font
  int defaultFontIndex = 0;

  auto applyCharFormat = [&]() -> QTextCharFormat {
    QTextCharFormat fmt;
    const CharState &cs = currentState.charState;

    fmt.setFontWeight(cs.bold ? QFont::Bold : QFont::Normal);
    fmt.setFontItalic(cs.italic);
    fmt.setFontUnderline(cs.underline);
    fmt.setFontStrikeOut(cs.strikethrough);

    // Font size: RTF uses half-points, Qt uses points
    if (cs.fontSize > 0) {
      fmt.setFontPointSize(cs.fontSize / 2.0);
    }

    // Font family
    if (cs.fontIndex >= 0 && cs.fontIndex < fontTable.size()) {
      fmt.setFontFamilies({fontTable[cs.fontIndex].name});
    }

    // Text color
    if (cs.colorIndex > 0 && cs.colorIndex <= colorTable.size()) {
      const ColorEntry &ce = colorTable[cs.colorIndex - 1];
      fmt.setForeground(QColor(ce.red, ce.green, ce.blue));
    }

    return fmt;
  };

  for (int ti = 0; ti < tokens.size(); ++ti) {
    const Token &tok = tokens[ti];
    ParseMode mode = modeStack.top();

    // Handle skip mode
    if (mode == ParseMode::SkipGroup) {
      if (tok.type == TokenType::GroupStart) {
        skipDepth++;
      } else if (tok.type == TokenType::GroupEnd) {
        skipDepth--;
        if (skipDepth <= 0) {
          modeStack.pop();
          if (!stateStack.isEmpty()) {
            currentState = stateStack.pop();
          }
        }
      }
      continue;
    }

    switch (tok.type) {
    case TokenType::GroupStart: {
      stateStack.push(currentState);

      // Check if next token is \* (ignorable destination)
      if (ti + 1 < tokens.size() &&
          tokens[ti + 1].type == TokenType::ControlWord &&
          tokens[ti + 1].word == QStringLiteral("*")) {
        // Check if the destination after \* is known
        if (ti + 2 < tokens.size() &&
            tokens[ti + 2].type == TokenType::ControlWord) {
          const QString &destWord = tokens[ti + 2].word;
          // Known ignorable destinations we want to skip
          if (destWord != QStringLiteral("fonttbl") &&
              destWord != QStringLiteral("colortbl") &&
              destWord != QStringLiteral("pn")) {
            modeStack.push(ParseMode::SkipGroup);
            skipDepth = 1;
            ti += 2; // skip \* and the destination word
            continue;
          }
        }
      }

      if (mode == ParseMode::FontTable) {
        // Beginning of a font entry sub-group
        currentFont = FontEntry();
        fontNameAccum.clear();
        modeStack.push(ParseMode::FontTable);
      } else {
        modeStack.push(mode);
      }
      break;
    }

    case TokenType::GroupEnd: {
      if (mode == ParseMode::FontTable) {
        // If we have a font name accumulated, save it
        if (!fontNameAccum.isEmpty()) {
          // Remove trailing semicolons and whitespace
          fontNameAccum = fontNameAccum.trimmed();
          if (fontNameAccum.endsWith(QLatin1Char(';'))) {
            fontNameAccum.chop(1);
          }
          currentFont.name = fontNameAccum.trimmed();
          // Ensure the font table is large enough
          while (fontTable.size() <= currentFont.id) {
            fontTable.append(FontEntry());
          }
          fontTable[currentFont.id] = currentFont;
          fontNameAccum.clear();
        }
      } else if (mode == ParseMode::ColorTable) {
        // Finalize any pending color entry
        if (colorHasComponent) {
          colorTable.append(currentColor);
          currentColor = ColorEntry();
          colorHasComponent = false;
        }
      }

      modeStack.pop();
      if (modeStack.isEmpty()) {
        modeStack.push(ParseMode::Normal);
      }

      if (!stateStack.isEmpty()) {
        currentState = stateStack.pop();
      }
      break;
    }

    case TokenType::ControlWord: {
      const QString &w = tok.word;

      if (w == QStringLiteral("rtf")) {
        seenRtfHeader = true;
        continue;
      }

      // Font table
      if (w == QStringLiteral("fonttbl")) {
        modeStack.pop();
        modeStack.push(ParseMode::FontTable);
        continue;
      }

      // Color table
      if (w == QStringLiteral("colortbl")) {
        modeStack.pop();
        modeStack.push(ParseMode::ColorTable);
        colorTable.clear();
        currentColor = ColorEntry();
        colorHasComponent = false;
        continue;
      }

      // Skip known destinations that we don't handle
      if (w == QStringLiteral("stylesheet") ||
          w == QStringLiteral("info") ||
          w == QStringLiteral("header") ||
          w == QStringLiteral("footer") ||
          w == QStringLiteral("headerl") ||
          w == QStringLiteral("headerr") ||
          w == QStringLiteral("footerl") ||
          w == QStringLiteral("footerr") ||
          w == QStringLiteral("pict") ||
          w == QStringLiteral("object") ||
          w == QStringLiteral("field") ||
          w == QStringLiteral("fldinst") ||
          w == QStringLiteral("datafield") ||
          w == QStringLiteral("mmathPr") ||
          w == QStringLiteral("generator") ||
          w == QStringLiteral("listtable") ||
          w == QStringLiteral("listoverridetable") ||
          w == QStringLiteral("rsidtbl") ||
          w == QStringLiteral("pgdsctbl") ||
          w == QStringLiteral("latentstyles")) {
        // If we're inside a group for this, skip until group end
        if (!stateStack.isEmpty()) {
          modeStack.pop();
          modeStack.push(ParseMode::SkipGroup);
          skipDepth = 1;
        }
        continue;
      }

      // Font table mode: handle font entries
      if (mode == ParseMode::FontTable) {
        if (w == QStringLiteral("f") && tok.hasParam) {
          currentFont.id = tok.parameter;
        }
        // Skip font family types (fnil, froman, fswiss, etc.)
        // Skip fcharset, fprq, etc.
        continue;
      }

      // Color table mode: handle color entries
      if (mode == ParseMode::ColorTable) {
        if (w == QStringLiteral("red") && tok.hasParam) {
          currentColor.red = tok.parameter;
          colorHasComponent = true;
        } else if (w == QStringLiteral("green") && tok.hasParam) {
          currentColor.green = tok.parameter;
          colorHasComponent = true;
        } else if (w == QStringLiteral("blue") && tok.hasParam) {
          currentColor.blue = tok.parameter;
          colorHasComponent = true;
        }
        continue;
      }

      // Normal mode: handle formatting control words

      // Default font
      if (w == QStringLiteral("deff") && tok.hasParam) {
        defaultFontIndex = tok.parameter;
        currentState.charState.fontIndex = defaultFontIndex;
        continue;
      }

      // Font selection
      if (w == QStringLiteral("f") && tok.hasParam) {
        currentState.charState.fontIndex = tok.parameter;
        continue;
      }

      // Font size (in half-points)
      if (w == QStringLiteral("fs") && tok.hasParam) {
        currentState.charState.fontSize = tok.parameter;
        continue;
      }

      // Bold
      if (w == QStringLiteral("b")) {
        currentState.charState.bold = tok.hasParam ? (tok.parameter != 0) : true;
        continue;
      }

      // Italic
      if (w == QStringLiteral("i")) {
        currentState.charState.italic = tok.hasParam ? (tok.parameter != 0) : true;
        continue;
      }

      // Underline
      if (w == QStringLiteral("ul")) {
        currentState.charState.underline = tok.hasParam ? (tok.parameter != 0) : true;
        continue;
      }
      if (w == QStringLiteral("ulnone")) {
        currentState.charState.underline = false;
        continue;
      }

      // Strikethrough
      if (w == QStringLiteral("strike")) {
        currentState.charState.strikethrough = tok.hasParam ? (tok.parameter != 0) : true;
        continue;
      }

      // Text color
      if (w == QStringLiteral("cf") && tok.hasParam) {
        currentState.charState.colorIndex = tok.parameter;
        continue;
      }

      // Paragraph break
      if (w == QStringLiteral("par")) {
        if (firstParagraph) {
          firstParagraph = false;
        }

        // Check if current paragraph should be a list item
        if (pendingListItem) {
          QTextBlockFormat bfmt;
          bfmt.setIndent(1);
          cursor.setBlockFormat(bfmt);

          QTextListFormat listFmt;
          listFmt.setStyle(QTextListFormat::ListDisc);
          listFmt.setIndent(1);
          cursor.createList(listFmt);
          pendingListItem = false;
        }

        cursor.insertBlock();
        continue;
      }

      // Paragraph reset
      if (w == QStringLiteral("pard")) {
        currentState.charState = CharState();
        currentState.charState.fontIndex = defaultFontIndex;
        currentState.paraState = ParaState();
        pendingListItem = false;
        continue;
      }

      // Line break
      if (w == QStringLiteral("line")) {
        cursor.insertText(QStringLiteral("\n"));
        continue;
      }

      // Tab
      if (w == QStringLiteral("tab")) {
        cursor.insertText(QStringLiteral("\t"));
        continue;
      }

      // List-related: detect bullets
      if (w == QStringLiteral("pnlvlblt")) {
        pendingListItem = true;
        continue;
      }

      // Left indent: \liN (in twips)
      if (w == QStringLiteral("li") && tok.hasParam) {
        currentState.paraState.leftIndent = tok.parameter;
        continue;
      }

      // First-line indent: \fiN (in twips)
      if (w == QStringLiteral("fi") && tok.hasParam) {
        currentState.paraState.firstLineIndent = tok.parameter;
        continue;
      }

      // Paragraph-level list number bullets from LibreOffice
      // \ls and \ilvl indicate list override and level
      if (w == QStringLiteral("ls") && tok.hasParam) {
        pendingListItem = true;
        continue;
      }

      // \pntext group — bullet text representation, skip it
      if (w == QStringLiteral("pntext")) {
        if (!stateStack.isEmpty()) {
          modeStack.pop();
          modeStack.push(ParseMode::SkipGroup);
          skipDepth = 1;
        }
        continue;
      }

      // \pn — list number properties group, skip its content
      if (w == QStringLiteral("pn")) {
        // We'll look at the sub-properties for bullet detection
        // but for now just note it
        continue;
      }

      // Unicode character: \uN followed by a replacement char
      if (w == QStringLiteral("u") && tok.hasParam) {
        int codepoint = tok.parameter;
        if (codepoint < 0) {
          codepoint += 65536;
        }
        QTextCharFormat fmt = applyCharFormat();
        cursor.insertText(QString(QChar(codepoint)), fmt);

        // Skip the ANSI replacement character (usually next text char)
        if (ti + 1 < tokens.size() &&
            tokens[ti + 1].type == TokenType::Text &&
            tokens[ti + 1].text.size() == 1) {
          ti++;
        }
        continue;
      }

      // Unicode character count to skip after \u: \ucN
      // We handle this implicitly above

      // Ignore other control words we don't handle
      break;
    }

    case TokenType::Text: {
      if (mode == ParseMode::FontTable) {
        fontNameAccum += tok.text;
        continue;
      }

      if (mode == ParseMode::ColorTable) {
        // In color table, semicolons delimit entries
        const QString &txt = tok.text;
        for (int ci = 0; ci < txt.size(); ++ci) {
          if (txt[ci] == QLatin1Char(';')) {
            colorTable.append(currentColor);
            currentColor = ColorEntry();
            colorHasComponent = false;
          }
        }
        continue;
      }

      // Normal text: apply current formatting and insert
      if (mode == ParseMode::Normal) {
        if (firstParagraph) {
          firstParagraph = false;
        }
        QTextCharFormat fmt = applyCharFormat();
        cursor.insertText(tok.text, fmt);
      }
      break;
    }

    case TokenType::HexChar: {
      if (mode == ParseMode::FontTable) {
        // Some font names use hex characters
        fontNameAccum += QChar::fromLatin1(static_cast<char>(tok.hexValue));
        continue;
      }
      if (mode == ParseMode::ColorTable) {
        continue;
      }
      if (mode == ParseMode::Normal) {
        if (firstParagraph) {
          firstParagraph = false;
        }
        QTextCharFormat fmt = applyCharFormat();
        // Convert from Windows-1252 by default
        QChar ch = QChar::fromLatin1(static_cast<char>(tok.hexValue));
        cursor.insertText(QString(ch), fmt);
      }
      break;
    }
    }
  }

  // Apply formatting to the last paragraph if it was a list item
  if (pendingListItem) {
    QTextBlockFormat bfmt;
    bfmt.setIndent(1);
    cursor.setBlockFormat(bfmt);

    QTextListFormat listFmt;
    listFmt.setStyle(QTextListFormat::ListDisc);
    listFmt.setIndent(1);
    cursor.createList(listFmt);
  }

  return seenRtfHeader;
}

// ============================================================================
// RTF Writer
// ============================================================================

QByteArray RtfHandler::writeRtf(const QTextDocument *doc) {
  if (!doc)
    return QByteArray();

  // Collect all fonts and colors used in the document
  QStringList fontNames;
  QVector<QColor> colors;

  // Always have a default font
  fontNames.append(QStringLiteral("Sans Serif"));

  // First pass: collect fonts and colors
  QTextBlock block = doc->begin();
  while (block.isValid()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      QTextFragment fragment = it.fragment();
      if (!fragment.isValid())
        continue;

      QTextCharFormat fmt = fragment.charFormat();

      // Collect font
      QStringList families = fmt.fontFamilies().toStringList();
      QString family =
          families.isEmpty() ? QStringLiteral("Sans Serif") : families.first();
      if (!fontNames.contains(family)) {
        fontNames.append(family);
      }

      // Collect color
      if (fmt.foreground().style() != Qt::NoBrush) {
        QColor color = fmt.foreground().color();
        if (color.isValid() && color != QColor(Qt::black)) {
          if (!colors.contains(color)) {
            colors.append(color);
          }
        }
      }
    }
    block = block.next();
  }

  // Build the RTF output
  QByteArray rtf;
  rtf.append("{\\rtf1\\ansi\\ansicpg1252\\deff0\n");

  // Font table
  rtf.append("{\\fonttbl");
  for (int i = 0; i < fontNames.size(); ++i) {
    rtf.append("{\\f");
    rtf.append(QByteArray::number(i));
    rtf.append("\\fnil ");
    rtf.append(fontNames[i].toLatin1());
    rtf.append(";}");
  }
  rtf.append("}\n");

  // Color table (index 0 = auto/default with empty entry, then our colors)
  rtf.append("{\\colortbl ;");
  // Always add black as index 1
  rtf.append("\\red0\\green0\\blue0;");
  for (const QColor &c : colors) {
    rtf.append("\\red");
    rtf.append(QByteArray::number(c.red()));
    rtf.append("\\green");
    rtf.append(QByteArray::number(c.green()));
    rtf.append("\\blue");
    rtf.append(QByteArray::number(c.blue()));
    rtf.append(";");
  }
  rtf.append("}\n");

  // Helper: find font index
  auto fontIndex = [&](const QTextCharFormat &fmt) -> int {
    QStringList families = fmt.fontFamilies().toStringList();
    QString family =
        families.isEmpty() ? QStringLiteral("Sans Serif") : families.first();
    int idx = fontNames.indexOf(family);
    return idx >= 0 ? idx : 0;
  };

  // Helper: find color index (1-based, 0 = auto)
  auto colorIndex = [&](const QTextCharFormat &fmt) -> int {
    if (fmt.foreground().style() == Qt::NoBrush)
      return 0;
    QColor color = fmt.foreground().color();
    if (!color.isValid() || color == QColor(Qt::black))
      return 1; // black is index 1
    int idx = colors.indexOf(color);
    return idx >= 0 ? idx + 2 : 0; // +2 because index 0 = auto, index 1 = black
  };

  // Helper: escape text for RTF
  auto escapeText = [](const QString &text) -> QByteArray {
    QByteArray result;
    for (const QChar &ch : text) {
      ushort code = ch.unicode();
      if (code == '\\') {
        result.append("\\\\");
      } else if (code == '{') {
        result.append("\\{");
      } else if (code == '}') {
        result.append("\\}");
      } else if (code > 127) {
        // Unicode character
        result.append("\\u");
        result.append(QByteArray::number(static_cast<int>(code)));
        result.append("?"); // ANSI replacement
      } else {
        result.append(static_cast<char>(code));
      }
    }
    return result;
  };

  // Second pass: write content
  block = doc->begin();
  bool firstBlock = true;

  while (block.isValid()) {
    if (!firstBlock) {
      rtf.append("\\par\n");
    }
    firstBlock = false;

    rtf.append("\\pard");

    // Check if this block is in a list
    QTextList *list = block.textList();
    if (list) {
      QTextListFormat listFmt = list->format();
      // Write bullet list markers
      rtf.append("\\fi-360\\li720 ");
      rtf.append("{\\pntext\\f0 \\'B7\\tab}");
      rtf.append("{\\*\\pn\\pnlvlblt{\\pntxtb\\'B7}}");
    }

    rtf.append(" ");

    // Write fragments
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      QTextFragment fragment = it.fragment();
      if (!fragment.isValid())
        continue;

      QTextCharFormat fmt = fragment.charFormat();
      QString text = fragment.text();

      // Open a group for this fragment's formatting
      rtf.append("{");

      // Font
      int fi = fontIndex(fmt);
      rtf.append("\\f");
      rtf.append(QByteArray::number(fi));

      // Font size (in half-points)
      qreal ptSize = fmt.fontPointSize();
      if (ptSize > 0) {
        rtf.append("\\fs");
        rtf.append(QByteArray::number(static_cast<int>(ptSize * 2)));
      }

      // Bold
      if (fmt.fontWeight() >= QFont::Bold) {
        rtf.append("\\b");
      }

      // Italic
      if (fmt.fontItalic()) {
        rtf.append("\\i");
      }

      // Underline
      if (fmt.fontUnderline()) {
        rtf.append("\\ul");
      }

      // Strikethrough
      if (fmt.fontStrikeOut()) {
        rtf.append("\\strike");
      }

      // Text color
      int ci = colorIndex(fmt);
      if (ci > 0) {
        rtf.append("\\cf");
        rtf.append(QByteArray::number(ci));
      }

      rtf.append(" ");
      rtf.append(escapeText(text));
      rtf.append("}");
    }

    // If block is empty, write at least a space to preserve the paragraph
    if (block.text().isEmpty() && !list) {
      rtf.append(" ");
    }

    block = block.next();
  }

  rtf.append("}\n");
  return rtf;
}
