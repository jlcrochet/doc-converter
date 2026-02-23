/*
 * doc-converter - Convert between document formats (Markdown, HTML, DOCX, ODT, EPUB)
 *
 * A lightweight pandoc alternative in C with minimal dependencies.
 * Optional zlib for DOCX/ODT/EPUB compression.
 *
 * Build with zlib:    cc -DHAVE_ZLIB doc-converter.c -lz
 * Build without zlib: cc doc-converter.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "lib/stdio_helpers.h"
#include "lookup/short_character_references.h"
#include "lookup/full_character_references.h"

static bool strict_mode = false;
static size_t max_bytes = 268435456;	// 0 = unlimited

// Codepoint mapping for short character references (index = enum value)
static const uint32_t short_character_reference_codepoints[] = {
	0, U'Æ', U'&', U'Á', U'Â', U'À', U'Å', U'Ã',
	U'Ä', U'©', U'Ç', U'Ð', U'É', U'Ê', U'È', U'Ë',
	U'>', U'Í', U'Î', U'Ì', U'Ï', U'<', U'Ñ', U'Ó',
	U'Ô', U'Ò', U'Ø', U'Õ', U'Ö', U'"', U'®', U'Þ',
	U'Ú', U'Û', U'Ù', U'Ü', U'Ý', U'á', U'â', U'´',
	U'æ', U'à', U'&', U'å', U'ã', U'ä', U'¦', U'ç',
	U'¸', U'¢', U'©', U'¤', U'°', U'÷', U'é', U'ê',
	U'è', U'ð', U'ë', U'½', U'¼', U'¾', U'>', U'í',
	U'î', U'¡', U'ì', U'¿', U'ï', U'«', U'<', U'¯',
	U'µ', U'·', 0xA0, U'¬', U'ñ', U'ó', U'ô', U'ò',
	U'ª', U'º', U'ø', U'õ', U'ö', U'¶', U'±', U'£',
	U'"', U'»', U'®', U'§', 0xAD, U'¹', U'²', U'³',
	U'ß', U'þ', U'×', U'ú', U'û', U'ù', U'¨', U'ü',
	U'ý', U'¥', U'ÿ'
};

// Codepoint mapping for full character references (index = enum value)
static const uint32_t full_character_reference_codepoints[] = {
	0, U'Æ', U'&', U'Á', U'Ă', U'Â', U'А', 0x1D504,
	U'À', U'Α', U'Ā', U'⩓', U'Ą', 0x1D538, 0x2061, U'Å',
	0x1D49C, U'≔', U'Ã', U'Ä', U'∖', U'⫧', U'⌆', U'Б',
	U'∵', U'ℬ', U'Β', 0x1D505, 0x1D539, U'˘', U'ℬ', U'≎',
	U'Ч', U'©', U'Ć', U'⋒', U'ⅅ', U'ℭ', U'Č', U'Ç',
	U'Ĉ', U'∰', U'Ċ', U'¸', U'·', U'ℭ', U'Χ', U'⊙',
	U'⊖', U'⊕', U'⊗', U'∲', U'"', 0x2019, U'∷', U'⩴',
	U'≡', U'∯', U'∮', U'ℂ', U'∐', U'∳', U'⨯', 0x1D49E,
	U'⋓', U'≍', U'ⅅ', U'⤑', U'Ђ', U'Ѕ', U'Џ', U'‡',
	U'↡', U'⫤', U'Ď', U'Д', U'∇', U'Δ', 0x1D507, U'´',
	U'˙', U'˝', U'`', U'˜', U'⋄', U'ⅆ', 0x1D53B, U'¨',
	0x20DC, U'≐', U'∯', U'¨', U'⇓', U'⇐', U'⇔', U'⫤',
	U'⟸', U'⟺', U'⟹', U'⇒', U'⊨', U'⇑', U'⇕', U'∥',
	U'↓', U'⤓', U'⇵', 0x311, U'⥐', U'⥞', U'↽', U'⥖',
	U'⥟', U'⇁', U'⥗', U'⊤', U'↧', U'⇓', 0x1D49F, U'Đ',
	U'Ŋ', U'Ð', U'É', U'Ě', U'Ê', U'Э', U'Ė', 0x1D508,
	U'È', U'∈', U'Ē', U'◻', U'▫', U'Ę', 0x1D53C, U'Ε',
	U'⩵', U'≂', U'⇌', U'ℰ', U'⩳', U'Η', U'Ë', U'∃',
	U'ⅇ', U'Ф', 0x1D509, U'◼', U'▪', 0x1D53D, U'∀', U'ℱ',
	U'ℱ', U'Ѓ', U'>', U'Γ', U'Ϝ', U'Ğ', U'Ģ', U'Ĝ',
	U'Г', U'Ġ', 0x1D50A, U'⋙', 0x1D53E, U'≥', U'⋛', U'≧',
	U'⪢', U'≷', U'⩾', U'≳', 0x1D4A2, U'≫', U'Ъ', U'ˇ',
	U'^', U'Ĥ', U'ℌ', U'ℋ', U'ℍ', U'─', U'ℋ', U'Ħ',
	U'≎', U'≏', U'Е', U'Ĳ', U'Ё', U'Í', U'Î', U'И',
	U'İ', U'ℑ', U'Ì', U'ℑ', U'Ī', U'ⅈ', U'⇒', U'∬',
	U'∫', U'⋂', 0x2063, 0x2062, U'Į', 0x1D540, U'Ι', U'ℐ',
	U'Ĩ', U'І', U'Ï', U'Ĵ', U'Й', 0x1D50D, 0x1D541, 0x1D4A5,
	U'Ј', U'Є', U'Х', U'Ќ', U'Κ', U'Ķ', U'К', 0x1D50E,
	0x1D542, 0x1D4A6, U'Љ', U'<', U'Ĺ', U'Λ', U'⟪', U'ℒ',
	U'↞', U'Ľ', U'Ļ', U'Л', U'⟨', U'←', U'⇤', U'⇆',
	U'⌈', U'⟦', U'⥡', U'⇃', U'⥙', U'⌊', U'↔', U'⥎',
	U'⊣', U'↤', U'⥚', U'⊲', U'⧏', U'⊴', U'⥑', U'⥠',
	U'↿', U'⥘', U'↼', U'⥒', U'⇐', U'⇔', U'⋚', U'≦',
	U'≶', U'⪡', U'⩽', U'≲', 0x1D50F, U'⋘', U'⇚', U'Ŀ',
	U'⟵', U'⟷', U'⟶', U'⟸', U'⟺', U'⟹', 0x1D543, U'↙',
	U'↘', U'ℒ', U'↰', U'Ł', U'≪', U'⤅', U'М', 0x205F,
	U'ℳ', 0x1D510, U'∓', 0x1D544, U'ℳ', U'Μ', U'Њ', U'Ń',
	U'Ň', U'Ņ', U'Н', 0x200B, 0x200B, 0x200B, 0x200B, U'≫',
	U'≪', 0xA, 0x1D511, 0x2060, 0xA0, U'ℕ', U'⫬', U'≢',
	U'≭', U'∦', U'∉', U'≠', U'≂', U'∄', U'≯', U'≱',
	U'≧', U'≫', U'≹', U'⩾', U'≵', U'≎', U'≏', U'⋪',
	U'⧏', U'⋬', U'≮', U'≰', U'≸', U'≪', U'⩽', U'≴',
	U'⪢', U'⪡', U'⊀', U'⪯', U'⋠', U'∌', U'⋫', U'⧐',
	U'⋭', U'⊏', U'⋢', U'⊐', U'⋣', U'⊂', U'⊈', U'⊁',
	U'⪰', U'⋡', U'≿', U'⊃', U'⊉', U'≁', U'≄', U'≇',
	U'≉', U'∤', 0x1D4A9, U'Ñ', U'Ν', U'Œ', U'Ó', U'Ô',
	U'О', U'Ő', 0x1D512, U'Ò', U'Ō', U'Ω', U'Ο', 0x1D546,
	U'"', 0x2018, U'⩔', 0x1D4AA, U'Ø', U'Õ', U'⨷', U'Ö',
	U'‾', U'⏞', U'⎴', U'⏜', U'∂', U'П', 0x1D513, U'Φ',
	U'Π', U'±', U'ℌ', U'ℙ', U'⪻', U'≺', U'⪯', U'≼',
	U'≾', U'″', U'∏', U'∷', U'∝', 0x1D4AB, U'Ψ', U'"',
	0x1D514, U'ℚ', 0x1D4AC, U'⤐', U'®', U'Ŕ', U'⟫', U'↠',
	U'⤖', U'Ř', U'Ŗ', U'Р', U'ℜ', U'∋', U'⇋', U'⥯',
	U'ℜ', U'Ρ', U'⟩', U'→', U'⇥', U'⇄', U'⌉', U'⟧',
	U'⥝', U'⇂', U'⥕', U'⌋', U'⊢', U'↦', U'⥛', U'⊳',
	U'⧐', U'⊵', U'⥏', U'⥜', U'↾', U'⥔', U'⇀', U'⥓',
	U'⇒', U'ℝ', U'⥰', U'⇛', U'ℛ', U'↱', U'⧴', U'Щ',
	U'Ш', U'Ь', U'Ś', U'⪼', U'Š', U'Ş', U'Ŝ', U'С',
	0x1D516, U'↓', U'←', U'→', U'↑', U'Σ', U'∘', 0x1D54A,
	U'√', U'□', U'⊓', U'⊏', U'⊑', U'⊐', U'⊒', U'⊔',
	0x1D4AE, U'⋆', U'⋐', U'⋐', U'⊆', U'≻', U'⪰', U'≽',
	U'≿', U'∋', U'∑', U'⋑', U'⊃', U'⊇', U'⋑', U'Þ',
	U'™', U'Ћ', U'Ц', 0x9, U'Τ', U'Ť', U'Ţ', U'Т',
	0x1D517, U'∴', U'Θ', 0x205F, 0x2009, U'∼', U'≃', U'≅',
	U'≈', 0x1D54B, 0x20DB, 0x1D4AF, U'Ŧ', U'Ú', U'↟', U'⥉',
	U'Ў', U'Ŭ', U'Û', U'У', U'Ű', 0x1D518, U'Ù', U'Ū',
	U'_', U'⏟', U'⎵', U'⏝', U'⋃', U'⊎', U'Ų', 0x1D54C,
	U'↑', U'⤒', U'⇅', U'↕', U'⥮', U'⊥', U'↥', U'⇑',
	U'⇕', U'↖', U'↗', U'ϒ', U'Υ', U'Ů', 0x1D4B0, U'Ũ',
	U'Ü', U'⊫', U'⫫', U'В', U'⊩', U'⫦', U'⋁', U'‖',
	U'‖', U'∣', U'|', U'❘', U'≀', 0x200A, 0x1D519, 0x1D54D,
	0x1D4B1, U'⊪', U'Ŵ', U'⋀', 0x1D51A, 0x1D54E, 0x1D4B2, 0x1D51B,
	U'Ξ', 0x1D54F, 0x1D4B3, U'Я', U'Ї', U'Ю', U'Ý', U'Ŷ',
	U'Ы', 0x1D51C, 0x1D550, 0x1D4B4, U'Ÿ', U'Ж', U'Ź', U'Ž',
	U'З', U'Ż', 0x200B, U'Ζ', U'ℨ', U'ℤ', 0x1D4B5, U'á',
	U'ă', U'∾', U'∾', U'∿', U'â', U'´', U'а', U'æ',
	0x2061, 0x1D51E, U'à', U'ℵ', U'ℵ', U'α', U'ā', U'⨿',
	U'&', U'∧', U'⩕', U'⩜', U'⩘', U'⩚', U'∠', U'⦤',
	U'∠', U'∡', U'⦨', U'⦩', U'⦪', U'⦫', U'⦬', U'⦭',
	U'⦮', U'⦯', U'∟', U'⊾', U'⦝', U'∢', U'Å', U'⍼',
	U'ą', 0x1D552, U'≈', U'⩰', U'⩯', U'≊', U'≋', 0x27,
	U'≈', U'≊', U'å', 0x1D4B6, U'*', U'≈', U'≍', U'ã',
	U'ä', U'∳', U'⨑', U'⫭', U'≌', U'϶', U'‵', U'∽',
	U'⋍', U'⊽', U'⌅', U'⌅', U'⎵', U'⎶', U'≌', U'б',
	U'„', U'∵', U'∵', U'⦰', U'϶', U'ℬ', U'β', U'ℶ',
	U'≬', 0x1D51F, U'⋂', U'◯', U'⋃', U'⨀', U'⨁', U'⨂',
	U'⨆', U'★', U'▽', U'△', U'⨄', U'⋁', U'⋀', U'⤍',
	U'⧫', U'▪', U'▴', U'▾', U'◂', U'▸', U'␣', U'▒',
	U'░', U'▓', U'█', U'=', U'≡', U'⌐', 0x1D553, U'⊥',
	U'⊥', U'⋈', U'╗', U'╔', U'╖', U'╓', U'═', U'╦',
	U'╩', U'╤', U'╧', U'╝', U'╚', U'╜', U'╙', U'║',
	U'╬', U'╣', U'╠', U'╫', U'╢', U'╟', U'⧉', U'╕',
	U'╒', U'┐', U'┌', U'─', U'╥', U'╨', U'┬', U'┴',
	U'⊟', U'⊞', U'⊠', U'╛', U'╘', U'┘', U'└', U'│',
	U'╪', U'╡', U'╞', U'┼', U'┤', U'├', U'‵', U'˘',
	U'¦', 0x1D4B7, U'⁏', U'∽', U'⋍', 0x5C, U'⧅', U'⟈',
	U'•', U'•', U'≎', U'⪮', U'≏', U'≏', U'ć', U'∩',
	U'⩄', U'⩉', U'⩋', U'⩇', U'⩀', U'∩', U'⁁', U'ˇ',
	U'⩍', U'č', U'ç', U'ĉ', U'⩌', U'⩐', U'ċ', U'¸',
	U'⦲', U'¢', U'·', 0x1D520, U'ч', U'✓', U'✓', U'χ',
	U'○', U'⧃', U'ˆ', U'≗', U'↺', U'↻', U'®', U'Ⓢ',
	U'⊛', U'⊚', U'⊝', U'≗', U'⨐', U'⫯', U'⧂', U'♣',
	U'♣', U':', U'≔', U'≔', U',', U'@', U'∁', U'∘',
	U'∁', U'ℂ', U'≅', U'⩭', U'∮', 0x1D554, U'∐', U'©',
	U'℗', U'↵', U'✗', 0x1D4B8, U'⫏', U'⫑', U'⫐', U'⫒',
	U'⋯', U'⤸', U'⤵', U'⋞', U'⋟', U'↶', U'⤽', U'∪',
	U'⩈', U'⩆', U'⩊', U'⊍', U'⩅', U'∪', U'↷', U'⤼',
	U'⋞', U'⋟', U'⋎', U'⋏', U'¤', U'↶', U'↷', U'⋎',
	U'⋏', U'∲', U'∱', U'⌭', U'⇓', U'⥥', U'†', U'ℸ',
	U'↓', U'‐', U'⊣', U'⤏', U'˝', U'ď', U'д', U'ⅆ',
	U'‡', U'⇊', U'⩷', U'°', U'δ', U'⦱', U'⥿', 0x1D521,
	U'⇃', U'⇂', U'⋄', U'⋄', U'♦', U'♦', U'¨', U'ϝ',
	U'⋲', U'÷', U'÷', U'⋇', U'⋇', U'ђ', U'⌞', U'⌍',
	U'$', 0x1D555, U'˙', U'≐', U'≑', U'∸', U'∔', U'⊡',
	U'⌆', U'↓', U'⇊', U'⇃', U'⇂', U'⤐', U'⌟', U'⌌',
	0x1D4B9, U'ѕ', U'⧶', U'đ', U'⋱', U'▿', U'▾', U'⇵',
	U'⥯', U'⦦', U'џ', U'⟿', U'⩷', U'≑', U'é', U'⩮',
	U'ě', U'≖', U'ê', U'≕', U'э', U'ė', U'ⅇ', U'≒',
	0x1D522, U'⪚', U'è', U'⪖', U'⪘', U'⪙', U'⏧', U'ℓ',
	U'⪕', U'⪗', U'ē', U'∅', U'∅', U'∅', 0x2004, 0x2005,
	0x2003, U'ŋ', 0x2002, U'ę', 0x1D556, U'⋕', U'⧣', U'⩱',
	U'ε', U'ε', U'ϵ', U'≖', U'≕', U'≂', U'⪖', U'⪕',
	U'=', U'≟', U'≡', U'⩸', U'⧥', U'≓', U'⥱', U'ℯ',
	U'≐', U'≂', U'η', U'ð', U'ë', U'€', U'!', U'∃',
	U'ℰ', U'ⅇ', U'≒', U'ф', U'♀', U'ﬃ', U'ﬀ', U'ﬄ',
	0x1D523, U'ﬁ', U'f', U'♭', U'ﬂ', U'▱', U'ƒ', 0x1D557,
	U'∀', U'⋔', U'⫙', U'⨍', U'½', U'⅓', U'¼', U'⅕',
	U'⅙', U'⅛', U'⅔', U'⅖', U'¾', U'⅗', U'⅜', U'⅘',
	U'⅚', U'⅝', U'⅞', U'⁄', U'⌢', 0x1D4BB, U'≧', U'⪌',
	U'ǵ', U'γ', U'ϝ', U'⪆', U'ğ', U'ĝ', U'г', U'ġ',
	U'≥', U'⋛', U'≥', U'≧', U'⩾', U'⩾', U'⪩', U'⪀',
	U'⪂', U'⪄', U'⋛', U'⪔', 0x1D524, U'≫', U'⋙', U'ℷ',
	U'ѓ', U'≷', U'⪒', U'⪥', U'⪤', U'≩', U'⪊', U'⪊',
	U'⪈', U'⪈', U'≩', U'⋧', 0x1D558, U'`', U'ℊ', U'≳',
	U'⪎', U'⪐', U'>', U'⪧', U'⩺', U'⋗', U'⦕', U'⩼',
	U'⪆', U'⥸', U'⋗', U'⋛', U'⪌', U'≷', U'≳', U'≩',
	U'≩', U'⇔', 0x200A, U'½', U'ℋ', U'ъ', U'↔', U'⥈',
	U'↭', U'ℏ', U'ĥ', U'♥', U'♥', U'…', U'⊹', 0x1D525,
	U'⤥', U'⤦', U'⇿', U'∻', U'↩', U'↪', 0x1D559, U'―',
	0x1D4BD, U'ℏ', U'ħ', U'⁃', U'‐', U'í', 0x2063, U'î',
	U'и', U'е', U'¡', U'⇔', 0x1D526, U'ì', U'ⅈ', U'⨌',
	U'∭', U'⧜', U'℩', U'ĳ', U'ī', U'ℑ', U'ℐ', U'ℑ',
	U'ı', U'⊷', U'Ƶ', U'∈', U'℅', U'∞', U'⧝', U'ı',
	U'∫', U'⊺', U'ℤ', U'⊺', U'⨗', U'⨼', U'ё', U'į',
	0x1D55A, U'ι', U'⨼', U'¿', 0x1D4BE, U'∈', U'⋹', U'⋵',
	U'⋴', U'⋳', U'∈', 0x2062, U'ĩ', U'і', U'ï', U'ĵ',
	U'й', 0x1D527, U'ȷ', 0x1D55B, 0x1D4BF, U'ј', U'є', U'κ',
	U'ϰ', U'ķ', U'к', 0x1D528, U'ĸ', U'х', U'ќ', 0x1D55C,
	0x1D4C0, U'⇚', U'⇐', U'⤛', U'⤎', U'≦', U'⪋', U'⥢',
	U'ĺ', U'⦴', U'ℒ', U'λ', U'⟨', U'⦑', U'⟨', U'⪅',
	U'«', U'←', U'⇤', U'⤟', U'⤝', U'↩', U'↫', U'⤹',
	U'⥳', U'↢', U'⪫', U'⤙', U'⪭', U'⪭', U'⤌', U'❲',
	U'{', U'[', U'⦋', U'⦏', U'⦍', U'ľ', U'ļ', U'⌈',
	U'{', U'л', U'⤶', U'"', U'„', U'⥧', U'⥋', U'↲',
	U'≤', U'←', U'↢', U'↽', U'↼', U'⇇', U'↔', U'⇆',
	U'⇋', U'↭', U'⋋', U'⋚', U'≤', U'≦', U'⩽', U'⩽',
	U'⪨', U'⩿', U'⪁', U'⪃', U'⋚', U'⪓', U'⪅', U'⋖',
	U'⋚', U'⪋', U'≶', U'≲', U'⥼', U'⌊', 0x1D529, U'≶',
	U'⪑', U'↽', U'↼', U'⥪', U'▄', U'љ', U'≪', U'⇇',
	U'⌞', U'⥫', U'◺', U'ŀ', U'⎰', U'⎰', U'≨', U'⪉',
	U'⪉', U'⪇', U'⪇', U'≨', U'⋦', U'⟬', U'⇽', U'⟦',
	U'⟵', U'⟷', U'⟼', U'⟶', U'↫', U'↬', U'⦅', 0x1D55D,
	U'⨭', U'⨴', U'∗', U'_', U'◊', U'◊', U'⧫', U'(',
	U'⦓', U'⇆', U'⌟', U'⇋', U'⥭', 0x200E, U'⊿', U'‹',
	0x1D4C1, U'↰', U'≲', U'⪍', U'⪏', U'[', 0x2018, 0x201A,
	U'ł', U'<', U'⪦', U'⩹', U'⋖', U'⋋', U'⋉', U'⥶',
	U'⩻', U'⦖', U'◃', U'⊴', U'◂', U'⥊', U'⥦', U'≨',
	U'≨', U'∺', U'¯', U'♂', U'✠', U'✠', U'↦', U'↦',
	U'↧', U'↤', U'↥', U'▮', U'⨩', U'м', U'—', U'∡',
	0x1D52A, U'℧', U'µ', U'∣', U'*', U'⫰', U'·', U'−',
	U'⊟', U'∸', U'⨪', U'⫛', U'…', U'∓', U'⊧', 0x1D55E,
	U'∓', 0x1D4C2, U'∾', U'μ', U'⊸', U'⊸', U'⋙', U'≫',
	U'≫', U'⇍', U'⇎', U'⋘', U'≪', U'≪', U'⇏', U'⊯',
	U'⊮', U'∇', U'ń', U'∠', U'≉', U'⩰', U'≋', U'ŉ',
	U'≉', U'♮', U'♮', U'ℕ', 0xA0, U'≎', U'≏', U'⩃',
	U'ň', U'ņ', U'≇', U'⩭', U'⩂', U'н', U'–', U'≠',
	U'⇗', U'⤤', U'↗', U'↗', U'≐', U'≢', U'⤨', U'≂',
	U'∄', U'∄', 0x1D52B, U'≧', U'≱', U'≱', U'≧', U'⩾',
	U'⩾', U'≵', U'≯', U'≯', U'⇎', U'↮', U'⫲', U'∋',
	U'⋼', U'⋺', U'∋', U'њ', U'⇍', U'≦', U'↚', U'‥',
	U'≰', U'↚', U'↮', U'≰', U'≦', U'⩽', U'⩽', U'≮',
	U'≴', U'≮', U'⋪', U'⋬', U'∤', 0x1D55F, U'¬', U'∉',
	U'⋹', U'⋵', U'∉', U'⋷', U'⋶', U'∌', U'∌', U'⋾',
	U'⋽', U'∦', U'∦', U'⫽', U'∂', U'⨔', U'⊀', U'⋠',
	U'⪯', U'⊀', U'⪯', U'⇏', U'↛', U'⤳', U'↝', U'↛',
	U'⋫', U'⋭', U'⊁', U'⋡', U'⪰', 0x1D4C3, U'∤', U'∦',
	U'≁', U'≄', U'≄', U'∤', U'∦', U'⋢', U'⋣', U'⊄',
	U'⫅', U'⊈', U'⊂', U'⊈', U'⫅', U'⊁', U'⪰', U'⊅',
	U'⫆', U'⊉', U'⊃', U'⊉', U'⫆', U'≹', U'ñ', U'≸',
	U'⋪', U'⋬', U'⋫', U'⋭', U'ν', U'#', U'№', 0x2007,
	U'⊭', U'⤄', U'≍', U'⊬', U'≥', U'>', U'⧞', U'⤂',
	U'≤', U'<', U'⊴', U'⤃', U'⊵', U'∼', U'⇖', U'⤣',
	U'↖', U'↖', U'⤧', U'Ⓢ', U'ó', U'⊛', U'⊚', U'ô',
	U'о', U'⊝', U'ő', U'⨸', U'⊙', U'⦼', U'œ', U'⦿',
	0x1D52C, U'˛', U'ò', U'⧁', U'⦵', U'Ω', U'∮', U'↺',
	U'⦾', U'⦻', U'‾', U'⧀', U'ō', U'ω', U'ο', U'⦶',
	U'⊖', 0x1D560, U'⦷', U'⦹', U'⊕', U'∨', U'↻', U'⩝',
	U'ℴ', U'ℴ', U'ª', U'º', U'⊶', U'⩖', U'⩗', U'⩛',
	U'ℴ', U'ø', U'⊘', U'õ', U'⊗', U'⨶', U'ö', U'⌽',
	U'∥', U'¶', U'∥', U'⫳', U'⫽', U'∂', U'п', U'%',
	U'.', U'‰', U'⊥', U'‱', 0x1D52D, U'φ', U'ϕ', U'ℳ',
	U'☎', U'π', U'⋔', U'ϖ', U'ℏ', U'ℎ', U'ℏ', U'+',
	U'⨣', U'⊞', U'⨢', U'∔', U'⨥', U'⩲', U'±', U'⨦',
	U'⨧', U'±', U'⨕', 0x1D561, U'£', U'≺', U'⪳', U'⪷',
	U'≼', U'⪯', U'≺', U'⪷', U'≼', U'⪯', U'⪹', U'⪵',
	U'⋨', U'≾', U'′', U'ℙ', U'⪵', U'⪹', U'⋨', U'∏',
	U'⌮', U'⌒', U'⌓', U'∝', U'∝', U'≾', U'⊰', 0x1D4C5,
	U'ψ', 0x2008, 0x1D52E, U'⨌', 0x1D562, U'⁗', 0x1D4C6, U'ℍ',
	U'⨖', U'?', U'≟', U'"', U'⇛', U'⇒', U'⤜', U'⤏',
	U'⥤', U'∽', U'ŕ', U'√', U'⦳', U'⟩', U'⦒', U'⦥',
	U'⟩', U'»', U'→', U'⥵', U'⇥', U'⤠', U'⤳', U'⤞',
	U'↪', U'↬', U'⥅', U'⥴', U'↣', U'↝', U'⤚', U'∶',
	U'ℚ', U'⤍', U'❳', U'}', U']', U'⦌', U'⦎', U'⦐',
	U'ř', U'ŗ', U'⌉', U'}', U'р', U'⤷', U'⥩', U'"',
	U'"', U'↳', U'ℜ', U'ℛ', U'ℜ', U'ℝ', U'▭', U'®',
	U'⥽', U'⌋', 0x1D52F, U'⇁', U'⇀', U'⥬', U'ρ', U'ϱ',
	U'→', U'↣', U'⇁', U'⇀', U'⇄', U'⇌', U'⇉', U'↝',
	U'⋌', U'˚', U'≓', U'⇄', U'⇌', 0x200F, U'⎱', U'⎱',
	U'⫮', U'⟭', U'⇾', U'⟧', U'⦆', 0x1D563, U'⨮', U'⨵',
	U')', U'⦔', U'⨒', U'⇉', U'›', 0x1D4C7, U'↱', U']',
	0x2019, 0x2019, U'⋌', U'⋊', U'▹', U'⊵', U'▸', U'⧎',
	U'⥨', U'℞', U'ś', 0x201A, U'≻', U'⪴', U'⪸', U'š',
	U'≽', U'⪰', U'ş', U'ŝ', U'⪶', U'⪺', U'⋩', U'⨓',
	U'≿', U'с', U'⋅', U'⊡', U'⩦', U'⇘', U'⤥', U'↘',
	U'↘', U'§', U';', U'⤩', U'∖', U'∖', U'✶', 0x1D530,
	U'⌢', U'♯', U'щ', U'ш', U'∣', U'∥', 0xAD, U'σ',
	U'ς', U'ς', U'∼', U'⩪', U'≃', U'≃', U'⪞', U'⪠',
	U'⪝', U'⪟', U'≆', U'⨤', U'⥲', U'←', U'∖', U'⨳',
	U'⧤', U'∣', U'⌣', U'⪪', U'⪬', U'⪬', U'ь', U'/',
	U'⧄', U'⌿', 0x1D564, U'♠', U'♠', U'∥', U'⊓', U'⊓',
	U'⊔', U'⊔', U'⊏', U'⊑', U'⊏', U'⊑', U'⊐', U'⊒',
	U'⊐', U'⊒', U'□', U'□', U'▪', U'▪', U'→', 0x1D4C8,
	U'∖', U'⌣', U'⋆', U'☆', U'★', U'ϵ', U'ϕ', U'¯',
	U'⊂', U'⫅', U'⪽', U'⊆', U'⫃', U'⫁', U'⫋', U'⊊',
	U'⪿', U'⥹', U'⊂', U'⊆', U'⫅', U'⊊', U'⫋', U'⫇',
	U'⫕', U'⫓', U'≻', U'⪸', U'≽', U'⪰', U'⪺', U'⪶',
	U'⋩', U'≿', U'∑', U'♪', U'¹', U'²', U'³', U'⊃',
	U'⫆', U'⪾', U'⫘', U'⊇', U'⫄', U'⟉', U'⫗', U'⥻',
	U'⫂', U'⫌', U'⊋', U'⫀', U'⊃', U'⊇', U'⫆', U'⊋',
	U'⫌', U'⫈', U'⫔', U'⫖', U'⇙', U'⤦', U'↙', U'↙',
	U'⤪', U'ß', U'⌖', U'τ', U'⎴', U'ť', U'ţ', U'т',
	0x20DB, U'⌕', 0x1D531, U'∴', U'∴', U'θ', U'ϑ', U'ϑ',
	U'≈', U'∼', 0x2009, U'≈', U'∼', U'þ', U'˜', U'×',
	U'⊠', U'⨱', U'⨰', U'∭', U'⤨', U'⊤', U'⌶', U'⫱',
	0x1D565, U'⫚', U'⤩', U'‴', U'™', U'▵', U'▿', U'◃',
	U'⊴', U'≜', U'▹', U'⊵', U'◬', U'≜', U'⨺', U'⨹',
	U'⧍', U'⨻', U'⏢', 0x1D4C9, U'ц', U'ћ', U'ŧ', U'≬',
	U'↞', U'↠', U'⇑', U'⥣', U'ú', U'↑', U'ў', U'ŭ',
	U'û', U'у', U'⇅', U'ű', U'⥮', U'⥾', 0x1D532, U'ù',
	U'↿', U'↾', U'▀', U'⌜', U'⌜', U'⌏', U'◸', U'ū',
	U'¨', U'ų', 0x1D566, U'↑', U'↕', U'↿', U'↾', U'⊎',
	U'υ', U'ϒ', U'υ', U'⇈', U'⌝', U'⌝', U'⌎', U'ů',
	U'◹', 0x1D4CA, U'⋰', U'ũ', U'▵', U'▴', U'⇈', U'ü',
	U'⦧', U'⇕', U'⫨', U'⫩', U'⊨', U'⦜', U'ϵ', U'ϰ',
	U'∅', U'ϕ', U'ϖ', U'∝', U'↕', U'ϱ', U'ς', U'⊊',
	U'⫋', U'⊋', U'⫌', U'ϑ', U'⊲', U'⊳', U'в', U'⊢',
	U'∨', U'⊻', U'≚', U'⋮', U'|', U'|', 0x1D533, U'⊲',
	U'⊂', U'⊃', 0x1D567, U'∝', U'⊳', 0x1D4CB, U'⫋', U'⊊',
	U'⫌', U'⊋', U'⦚', U'ŵ', U'⩟', U'∧', U'≙', U'℘',
	0x1D534, 0x1D568, U'℘', U'≀', U'≀', 0x1D4CC, U'⋂', U'◯',
	U'⋃', U'▽', 0x1D535, U'⟺', U'⟷', U'ξ', U'⟸', U'⟵',
	U'⟼', U'⋻', U'⨀', 0x1D569, U'⨁', U'⨂', U'⟹', U'⟶',
	0x1D4CD, U'⨆', U'⨄', U'△', U'⋁', U'⋀', U'ý', U'я',
	U'ŷ', U'ы', U'¥', 0x1D536, U'ї', 0x1D56A, 0x1D4CE, U'ю',
	U'ÿ', U'ź', U'ž', U'з', U'ż', U'ℨ', U'ζ', 0x1D537,
	U'ж', U'⇝', 0x1D56B, 0x1D4CF, 0x200D, 0x200C
};

// ============================================================================
// Document AST
// ============================================================================

// Inline element types (text formatting within a paragraph)
enum inline_type {
	INLINE_TEXT,        // Plain text
	INLINE_BOLD,        // **bold**
	INLINE_ITALIC,      // *italic*
	INLINE_CODE,        // `code`
	INLINE_LINK,        // [text](url)
	INLINE_IMAGE,       // ![alt](url)
	INLINE_LINEBREAK,   // Hard line break
};

struct inline_node {
	enum inline_type type;
	char *text;              // Text content, or URL for links/images
	char *title;             // Link title or image alt text
	struct inline_node *children;  // For bold/italic containing other inlines
	struct inline_node *next;
};

// Block element types
enum block_type {
	BLOCK_PARAGRAPH,
	BLOCK_HEADING,      // Level 1-6
	BLOCK_CODE_BLOCK,   // Fenced or indented code
	BLOCK_BLOCKQUOTE,
	BLOCK_LIST,         // Ordered or unordered
	BLOCK_LIST_ITEM,
	BLOCK_THEMATIC_BREAK,  // Horizontal rule
	BLOCK_TABLE,
	BLOCK_TABLE_ROW,
	BLOCK_TABLE_CELL,
};

	struct block_node {
		enum block_type type;

	// Type-specific data
	union {
		struct {
			int level;  // 1-6 for headings
		} heading;
		struct {
			char *language;  // Optional language for syntax highlighting
			char *code;      // Raw code content
		} code_block;
		struct {
			bool ordered;
			int start;       // Starting number for ordered lists
		} list;
		struct {
			bool is_header;  // Is this row a header row?
		} table_row;
		struct {
			enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT, ALIGN_DEFAULT } align;
			int colspan;
			int rowspan;
		} table_cell;
	};

	struct inline_node *inlines;  // Inline content (for paragraphs, headings, cells)
	struct block_node *children;  // Child blocks (for lists, blockquotes, tables)
	struct block_node *next;      // Next sibling block
};

static bool doc_contains_tables(struct block_node *node)
{
	while (node) {
		if (node->type == BLOCK_TABLE || node->type == BLOCK_TABLE_ROW || node->type == BLOCK_TABLE_CELL)
			return true;
		if (node->children && doc_contains_tables(node->children))
			return true;
		node = node->next;
	}
	return false;
}

// Document root
struct document {
	struct block_node *blocks;
	// Metadata (optional, for future use)
	char *title;
	char *author;
};

// ============================================================================
// Arena allocator (chunked to avoid realloc invalidating pointers)
// ============================================================================

struct arena_chunk {
	struct arena_chunk *next;
	size_t used;
	size_t cap;
	char data[];
};

static struct {
	struct arena_chunk *head;
	size_t chunk_size;
} arena;

static void arena_init(size_t chunk_size)
{
	arena.chunk_size = chunk_size;
	if (chunk_size > SIZE_MAX - sizeof(struct arena_chunk)) {
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	arena.head = malloc(sizeof(struct arena_chunk) + chunk_size);
	if (!arena.head) {
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	arena.head->next = NULL;
	arena.head->used = 0;
	arena.head->cap = chunk_size;
}

static void *arena_alloc(size_t size)
{
	// Align to 8 bytes
	size = (size + 7u) & ~(size_t)7u;

	struct arena_chunk *chunk = arena.head;

	// If current chunk doesn't have space, allocate a new one
	if (chunk->used + size > chunk->cap) {
		size_t new_cap = arena.chunk_size;
		if (size > new_cap)
			new_cap = size;  // Large allocation gets its own chunk

		if (new_cap > SIZE_MAX - sizeof(struct arena_chunk)) {
			PUTS_ERR("Error: out of memory\n");
			exit(1);
		}
		struct arena_chunk *new_chunk = malloc(sizeof(struct arena_chunk) + new_cap);
		if (!new_chunk) {
			PUTS_ERR("Error: out of memory\n");
			exit(1);
		}
		new_chunk->next = chunk;
		new_chunk->used = 0;
		new_chunk->cap = new_cap;
		arena.head = new_chunk;
		chunk = new_chunk;
	}

	void *p = chunk->data + chunk->used;
	chunk->used += size;
	return p;
}

static void *arena_zalloc(size_t size)
{
	void *p = arena_alloc(size);
	memset(p, 0, size);
	return p;
}

static char *arena_strdup(const char *s)
{
	size_t len = strlen(s);
	char *copy = arena_alloc(len + 1);
	memcpy(copy, s, len + 1);
	return copy;
}

static char *arena_strndup(const char *s, size_t n)
{
	char *copy = arena_alloc(n + 1);
	if (n > 0) {
		if (!s) {
			PUTS_ERR("Error: invalid string\n");
			exit(1);
		}
		memcpy(copy, s, n);
	}
	copy[n] = '\0';
	return copy;
}

// ============================================================================
// AST helpers
// ============================================================================

static struct inline_node *inline_new(enum inline_type type)
{
	struct inline_node *node = arena_zalloc(sizeof(*node));
	node->type = type;
	return node;
}

static struct block_node *block_new(enum block_type type)
{
	struct block_node *node = arena_zalloc(sizeof(*node));
	node->type = type;
	return node;
}

static struct inline_node *inline_clone(struct inline_node *src)
{
	if (!src)
		return NULL;

	struct inline_node *head = NULL;
	struct inline_node *tail = NULL;

	while (src) {
		struct inline_node *node = inline_new(src->type);
		node->text = src->text;
		node->title = src->title;
		node->children = inline_clone(src->children);

		if (tail) {
			tail->next = node;
		} else {
			head = node;
		}
		tail = node;
		src = src->next;
	}

	return head;
}

static struct inline_node *table_row_to_inlines(struct block_node *row)
{
	struct inline_node *head = NULL;
	struct inline_node *tail = NULL;
	bool first_cell = true;

	for (struct block_node *cell = row->children; cell; cell = cell->next) {
		if (cell->type != BLOCK_TABLE_CELL)
			continue;

		if (!first_cell) {
			struct inline_node *sep = inline_new(INLINE_TEXT);
			sep->text = arena_strdup(" | ");
			if (tail) {
				tail->next = sep;
			} else {
				head = sep;
			}
			tail = sep;
		}
		first_cell = false;

		struct inline_node *cl = inline_clone(cell->inlines);
		if (cl) {
			if (tail) {
				tail->next = cl;
			} else {
				head = cl;
			}
			while (tail && tail->next)
				tail = tail->next;
			if (!tail) {
				tail = cl;
				while (tail->next)
					tail = tail->next;
			}
		}
	}

	return head;
}

static struct block_node *table_to_paragraphs(struct block_node *table)
{
	struct block_node *head = NULL;
	struct block_node *tail = NULL;

	for (struct block_node *row = table->children; row; row = row->next) {
		if (row->type != BLOCK_TABLE_ROW)
			continue;

		struct block_node *para = block_new(BLOCK_PARAGRAPH);
		para->inlines = table_row_to_inlines(row);

		if (tail) {
			tail->next = para;
		} else {
			head = para;
		}
		tail = para;
	}

	return head;
}

static void flatten_tables_in_blocks(struct block_node **head)
{
	struct block_node **cur = head;
	while (*cur) {
		struct block_node *node = *cur;

		if (node->type == BLOCK_TABLE) {
			struct block_node *replacement = table_to_paragraphs(node);
			if (!replacement) {
				*cur = node->next;
				continue;
			}
			struct block_node *end = replacement;
			while (end->next)
				end = end->next;
			end->next = node->next;
			*cur = replacement;
			cur = &end->next;
			continue;
		}

		if (node->children)
			flatten_tables_in_blocks(&node->children);

		cur = &node->next;
	}
}

// ============================================================================
// Growable buffer for building strings
// ============================================================================

struct buffer {
	char *data;
	size_t len;
	size_t cap;
	FILE *stream;
};

static void buf_init(struct buffer *buf)
{
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
	buf->stream = NULL;
}

static void buf_init_stream(struct buffer *buf, FILE *stream)
{
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
	buf->stream = stream;
}

static void buf_flush(struct buffer *buf)
{
	if (!buf->stream || buf->len == 0)
		return;
	if (WRITE_FILE(buf->data, buf->len, buf->stream) != buf->len) {
		PUTS_ERR("Error: write failed\n");
		exit(1);
	}
	buf->len = 0;
}

static void buf_free(struct buffer *buf)
{
	free(buf->data);
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
}

static void buf_grow(struct buffer *buf, size_t needed)
{
	if (__builtin_expect(buf->len + needed <= buf->cap, 1)) return;
	if (needed > SIZE_MAX - buf->len) {
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	if (buf->stream) {
		buf_flush(buf);
		if (needed <= buf->cap)
			return;
	}
	size_t min_cap = buf->len + needed;
	size_t new_cap = buf->cap ? buf->cap : 256;
	while (new_cap < min_cap) {
		if (new_cap > SIZE_MAX / 2) {
			new_cap = min_cap;
			break;
		}
		new_cap *= 2;
	}
	void *new_data = realloc(buf->data, new_cap);
	if (!new_data) {
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	buf->data = new_data;
	buf->cap = new_cap;
}

static inline void buf_putc(struct buffer *buf, char c)
{
	if (__builtin_expect(buf->len >= buf->cap, 0))
		buf_grow(buf, 1);
	buf->data[buf->len++] = c;
}

static void buf_puts(struct buffer *buf, const char *s)
{
	size_t len = strlen(s);
	buf_grow(buf, len);
	memcpy(buf->data + buf->len, s, len);
	buf->len += len;
}

static void buf_write(struct buffer *buf, const char *s, size_t len)
{
	buf_grow(buf, len);
	memcpy(buf->data + buf->len, s, len);
	buf->len += len;
}

static void buf_printf(struct buffer *buf, const char *fmt, ...)
{
	va_list args, args_copy;
	va_start(args, fmt);

	// Try with current space
	buf_grow(buf, 64);
	size_t avail = buf->cap - buf->len;
	va_copy(args_copy, args);
	int n = vsnprintf(buf->data + buf->len, avail, fmt, args);
	va_end(args);

	if (n < 0) {
		va_end(args_copy);
		PUTS_ERR("Error: formatting failed\n");
		exit(1);
	}

	if ((size_t)n >= avail) {
		// Need more space, use the copy
		size_t needed = (size_t)n + 1;
		buf_grow(buf, needed);
		int n2 = vsnprintf(buf->data + buf->len, needed, fmt, args_copy);
		if (n2 < 0) {
			va_end(args_copy);
			PUTS_ERR("Error: formatting failed\n");
			exit(1);
		}
		n = n2;
	}
	va_end(args_copy);
	buf->len += (size_t)n;
}

// Finish buffer and return arena-allocated copy
static char *buf_finish(struct buffer *buf)
{
	if (buf->stream) {
		PUTS_ERR("Error: cannot finish streaming buffer\n");
		exit(1);
	}
	char *result = arena_strndup(buf->data, buf->len);
	buf_free(buf);
	return result;
}

// Finish buffer and return malloc'd data (for final output)
static char *buf_finish_malloc(struct buffer *buf)
{
	if (buf->stream) {
		buf_flush(buf);
		buf_free(buf);
		return NULL;
	}
	buf_putc(buf, '\0');
	char *result = buf->data;
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
	return result;
}

// Scratch space for inline parsing (stack-like)
static struct {
	size_t *data;
	size_t len;
	size_t cap;
} inline_scratch;

static FILE *output_stream = NULL;

static void buf_init_output(struct buffer *buf)
{
	if (output_stream)
		buf_init_stream(buf, output_stream);
	else
		buf_init(buf);
}

static size_t scratch_mark(void)
{
	return inline_scratch.len;
}

static size_t *scratch_alloc(size_t count)
{
	if (count == 0)
		return NULL;

	if (count > SIZE_MAX - inline_scratch.len) {
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}

	if (inline_scratch.len + count > inline_scratch.cap) {
		size_t new_cap = inline_scratch.cap ? inline_scratch.cap * 2 : 1024;
		size_t min_cap = inline_scratch.len + count;
		while (new_cap < min_cap) {
			if (new_cap > SIZE_MAX / 2) {
				new_cap = min_cap;
				break;
			}
			new_cap *= 2;
		}
		if (new_cap > SIZE_MAX / sizeof(size_t)) {
			PUTS_ERR("Error: out of memory\n");
			exit(1);
		}
		size_t *new_data = realloc(inline_scratch.data, new_cap * sizeof(size_t));
		if (!new_data) {
			PUTS_ERR("Error: out of memory\n");
			exit(1);
		}
		inline_scratch.data = new_data;
		inline_scratch.cap = new_cap;
	}

	size_t *p = inline_scratch.data + inline_scratch.len;
	inline_scratch.len += count;
	return p;
}

static void scratch_reset(size_t mark)
{
	inline_scratch.len = mark;
}

// ============================================================================
// Markdown Parser
// ============================================================================

struct md_parser {
	const char *input;
	size_t pos;
	size_t len;
};

static inline char md_peek(struct md_parser *p)
{
	return p->pos < p->len ? p->input[p->pos] : '\0';
}

static inline char md_peek_at(struct md_parser *p, size_t offset)
{
	size_t i = p->pos + offset;
	return i < p->len ? p->input[i] : '\0';
}

static inline char md_next(struct md_parser *p)
{
	return p->pos < p->len ? p->input[p->pos++] : '\0';
}

static inline void md_skip(struct md_parser *p, size_t n)
{
	p->pos += n;
	if (p->pos > p->len) p->pos = p->len;
}

static inline bool md_eof(struct md_parser *p)
{
	return p->pos >= p->len;
}

// Skip whitespace (not newlines)
static void md_skip_spaces(struct md_parser *p)
{
	while (md_peek(p) == ' ' || md_peek(p) == '\t')
		md_next(p);
}

// Count consecutive characters
static size_t md_count_char(struct md_parser *p, char c)
{
	size_t count = 0;
	while (md_peek_at(p, count) == c)
		count++;
	return count;
}

// Check if rest of line is blank
static bool md_rest_is_blank(struct md_parser *p)
{
	size_t i = p->pos;
	while (i < p->len && p->input[i] != '\n') {
		if (p->input[i] != ' ' && p->input[i] != '\t')
			return false;
		i++;
	}
	return true;
}

// Skip to end of line (past the newline)
static void md_skip_line(struct md_parser *p)
{
	while (!md_eof(p) && md_peek(p) != '\n')
		md_next(p);
	if (md_peek(p) == '\n')
		md_next(p);
}

// Get current line content (without newline)
static char *md_get_line(struct md_parser *p)
{
	size_t start = p->pos;
	while (!md_eof(p) && md_peek(p) != '\n')
		md_next(p);
	char *line = arena_strndup(p->input + start, p->pos - start);
	if (md_peek(p) == '\n')
		md_next(p);
	return line;
}

static struct inline_node *md_parse_inlines(const char *text, size_t len);

static inline const char *md_trim_left(const char *s, const char *end)
{
	while (s < end && (*s == ' ' || *s == '\t'))
		s++;
	return s;
}

static inline const char *md_trim_right(const char *start, const char *end)
{
	while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
		end--;
	return end;
}

static bool md_peek_line_bounds(struct md_parser *p, size_t pos, const char **out_start, const char **out_end, size_t *out_next_pos)
{
	if (pos > p->len)
		pos = p->len;

	size_t i = pos;
	while (i < p->len && p->input[i] != '\n')
		i++;

	*out_start = p->input + pos;
	*out_end = p->input + i;
	*out_next_pos = (i < p->len && p->input[i] == '\n') ? i + 1 : i;
	return true;
}

static bool md_parse_pipe_table_separator(const char *line, size_t len, int *aligns, size_t cap_aligns, size_t *out_cols)
{
	const char *s = line;
	const char *end = line + len;
	s = md_trim_left(s, end);
	end = md_trim_right(s, end);
	if (s >= end)
		return false;

	size_t cols = 0;
	bool saw_pipe = false;

	const char *p = s;
	if (p < end && *p == '|') {
		saw_pipe = true;
		p++;
	}

	while (p <= end) {
		const char *seg_start = p;
		while (p < end && *p != '|')
			p++;
		const char *seg_end = p;
		if (p < end && *p == '|') {
			saw_pipe = true;
			p++;
		}

		seg_start = md_trim_left(seg_start, seg_end);
		seg_end = md_trim_right(seg_start, seg_end);
		if (seg_start >= seg_end) {
			if (p >= end)
				break;
			continue;
		}

		bool left_colon = (*seg_start == ':');
		bool right_colon = (seg_end > seg_start && seg_end[-1] == ':');
		if (left_colon) seg_start++;
		if (right_colon && seg_end > seg_start) seg_end--;

		size_t dash_count = 0;
		for (const char *q = seg_start; q < seg_end; q++) {
			char c = *q;
			if (c == '-')
				dash_count++;
			else if (c == ' ' || c == '\t')
				continue;
			else
				return false;
		}

		if (dash_count < 3)
			return false;

		if (cols < cap_aligns) {
			if (left_colon && right_colon)
				aligns[cols] = ALIGN_CENTER;
			else if (right_colon)
				aligns[cols] = ALIGN_RIGHT;
			else if (left_colon)
				aligns[cols] = ALIGN_LEFT;
			else
				aligns[cols] = ALIGN_DEFAULT;
		}
		cols++;

		if (p >= end)
			break;
	}

	if (!saw_pipe || cols == 0)
		return false;

	*out_cols = cols;
	return true;
}

static bool md_is_pipe_table_start(struct md_parser *p)
{
	const char *l1, *l1_end, *l2, *l2_end;
	size_t next;
	md_peek_line_bounds(p, p->pos, &l1, &l1_end, &next);

	const char *t1 = md_trim_left(l1, l1_end);
	const char *e1 = md_trim_right(t1, l1_end);
	if (t1 >= e1)
		return false;
	if (memchr(t1, '|', (size_t)(e1 - t1)) == NULL)
		return false;

	if (next >= p->len)
		return false;
	md_peek_line_bounds(p, next, &l2, &l2_end, &next);

	int aligns[256];
	size_t cols = 0;
	return md_parse_pipe_table_separator(l2, (size_t)(l2_end - l2), aligns, sizeof(aligns) / sizeof(aligns[0]), &cols);
}

static struct block_node *md_parse_pipe_table_row(const char *line, int *aligns, size_t cols, bool is_header)
{
	struct block_node *row = block_new(BLOCK_TABLE_ROW);
	row->table_row.is_header = is_header;

	const char *s = line;
	const char *end = line + strlen(line);
	s = md_trim_left(s, end);
	end = md_trim_right(s, end);
	if (s < end && *s == '|')
		s++;

	struct block_node **cell_tail = &row->children;
	size_t col = 0;
	const char *p = s;
	while (p <= end) {
		const char *cell_start = p;
		while (p < end && *p != '|')
			p++;
		const char *cell_end = p;
		if (p < end && *p == '|')
			p++;

		cell_start = md_trim_left(cell_start, cell_end);
		cell_end = md_trim_right(cell_start, cell_end);

			struct block_node *cell = block_new(BLOCK_TABLE_CELL);
			if (col < cols) {
				int align = aligns[col];
				if (align == ALIGN_LEFT)
					cell->table_cell.align = ALIGN_LEFT;
				else if (align == ALIGN_CENTER)
					cell->table_cell.align = ALIGN_CENTER;
				else if (align == ALIGN_RIGHT)
					cell->table_cell.align = ALIGN_RIGHT;
				else
					cell->table_cell.align = ALIGN_DEFAULT;
			} else {
				cell->table_cell.align = ALIGN_DEFAULT;
			}
			cell->table_cell.colspan = 1;
			cell->table_cell.rowspan = 1;
			cell->inlines = md_parse_inlines(cell_start, (size_t)(cell_end - cell_start));
			*cell_tail = cell;
			cell_tail = &cell->next;

		col++;
		if (p >= end)
			break;
	}

	return row;
}

static struct block_node *md_parse_pipe_table(struct md_parser *p)
{
	if (!md_is_pipe_table_start(p))
		return NULL;

	char *header_line = md_get_line(p);
	char *sep_line = md_get_line(p);

	int aligns[256];
	size_t cols = 0;
	if (!md_parse_pipe_table_separator(sep_line, strlen(sep_line), aligns, sizeof(aligns) / sizeof(aligns[0]), &cols))
		return NULL;

	struct block_node *table = block_new(BLOCK_TABLE);
	struct block_node **row_tail = &table->children;

	struct block_node *header = md_parse_pipe_table_row(header_line, aligns, cols, true);
	*row_tail = header;
	row_tail = &header->next;

	while (!md_eof(p)) {
		if (md_peek(p) == '\n')
			break;

		const char *line, *line_end;
		size_t next;
		md_peek_line_bounds(p, p->pos, &line, &line_end, &next);
		const char *t = md_trim_left(line, line_end);
		const char *e = md_trim_right(t, line_end);
		if (t >= e)
			break;
		if (memchr(t, '|', (size_t)(e - t)) == NULL)
			break;

		char *row_line = md_get_line(p);
		struct block_node *row = md_parse_pipe_table_row(row_line, aligns, cols, false);
		*row_tail = row;
		row_tail = &row->next;
	}

	return table;
}

// Forward declarations
static struct inline_node *md_parse_inlines(const char *text, size_t len);
static struct block_node *md_parse_blocks(struct md_parser *p);

// Parse inline formatting
static struct inline_node *md_parse_inlines(const char *text, size_t len)
{
	struct inline_node *head = NULL;
	struct inline_node **tail = &head;
	struct buffer buf;
	buf_init(&buf);

	size_t *next_double_star = NULL;
	size_t *next_double_underscore = NULL;
	size_t *next_single_star = NULL;
	size_t *next_single_underscore = NULL;
	size_t *next_backtick = NULL;

	size_t mark = scratch_mark();
	if (len > 0) {
		size_t *scratch = scratch_alloc(5 * (len + 1));
		next_double_star = scratch;
		next_double_underscore = scratch + (len + 1);
		next_single_star = scratch + 2 * (len + 1);
		next_single_underscore = scratch + 3 * (len + 1);
		next_backtick = scratch + 4 * (len + 1);

		next_double_star[len] = len;
		next_double_underscore[len] = len;
		next_single_star[len] = len;
		next_single_underscore[len] = len;
		next_backtick[len] = len;

		for (size_t i = len; i-- > 0;) {
			next_double_star[i] = next_double_star[i + 1];
			next_double_underscore[i] = next_double_underscore[i + 1];
			next_single_star[i] = next_single_star[i + 1];
			next_single_underscore[i] = next_single_underscore[i + 1];
			next_backtick[i] = next_backtick[i + 1];

			if (text[i] == '*') {
				if (i + 1 < len && text[i + 1] == '*')
					next_double_star[i] = i;
				if (i + 1 >= len || text[i + 1] != '*')
					next_single_star[i] = i;
			} else if (text[i] == '_') {
				if (i + 1 < len && text[i + 1] == '_')
					next_double_underscore[i] = i;
				if (i + 1 >= len || text[i + 1] != '_')
					next_single_underscore[i] = i;
			} else if (text[i] == '`') {
				next_backtick[i] = i;
			}
		}
	}

	size_t i = 0;
	while (i < len) {
		char c = text[i];

		// Bold: **text** or __text__
		if ((c == '*' || c == '_') && i + 1 < len && text[i + 1] == c) {
			char delim = c;
			size_t start = i + 2;
			size_t end = (delim == '*')
				? next_double_star[start]
				: next_double_underscore[start];

			if (end < len) {
				// Flush text buffer
				if (buf.len > 0) {
					struct inline_node *node = inline_new(INLINE_TEXT);
					node->text = buf_finish(&buf);
					*tail = node;
					tail = &node->next;
					buf_init(&buf);
				}

				struct inline_node *node = inline_new(INLINE_BOLD);
				node->children = md_parse_inlines(text + start, end - start);
				*tail = node;
				tail = &node->next;
				i = end + 2;
				continue;
			}
		}

		// Italic: *text* or _text_
		if ((c == '*' || c == '_') && i + 1 < len && text[i + 1] != c) {
			char delim = c;
			size_t start = i + 1;
			size_t end = (delim == '*')
				? next_single_star[start]
				: next_single_underscore[start];

			if (end < len) {
				// Flush text buffer
				if (buf.len > 0) {
					struct inline_node *node = inline_new(INLINE_TEXT);
					node->text = buf_finish(&buf);
					*tail = node;
					tail = &node->next;
					buf_init(&buf);
				}

				struct inline_node *node = inline_new(INLINE_ITALIC);
				node->children = md_parse_inlines(text + start, end - start);
				*tail = node;
				tail = &node->next;
				i = end + 1;
				continue;
			}
		}

		// Inline code: `code`
		if (c == '`') {
			size_t start = i + 1;
			size_t end = next_backtick[start];

			if (end < len) {
				if (buf.len > 0) {
					struct inline_node *node = inline_new(INLINE_TEXT);
					node->text = buf_finish(&buf);
					*tail = node;
					tail = &node->next;
					buf_init(&buf);
				}

				struct inline_node *node = inline_new(INLINE_CODE);
				node->text = arena_strndup(text + start, end - start);
				*tail = node;
				tail = &node->next;
				i = end + 1;
				continue;
			}
		}

		// Link: [text](url) or [text](url "title")
		if (c == '[') {
			size_t text_start = i + 1;
			size_t text_end = text_start;
			int bracket_depth = 1;

			while (text_end < len && bracket_depth > 0) {
				if (text[text_end] == '[') bracket_depth++;
				else if (text[text_end] == ']') bracket_depth--;
				if (bracket_depth > 0) text_end++;
			}

			if (text_end < len && text[text_end] == ']' && text_end + 1 < len && text[text_end + 1] == '(') {
				size_t url_start = text_end + 2;
				size_t url_end = url_start;

				// Find closing )
				int paren_depth = 1;
				while (url_end < len && paren_depth > 0) {
					if (text[url_end] == '(') paren_depth++;
					else if (text[url_end] == ')') paren_depth--;
					if (paren_depth > 0) url_end++;
				}

				if (url_end < len) {
					if (buf.len > 0) {
						struct inline_node *node = inline_new(INLINE_TEXT);
						node->text = buf_finish(&buf);
						*tail = node;
						tail = &node->next;
						buf_init(&buf);
					}

					struct inline_node *node = inline_new(INLINE_LINK);
					node->children = md_parse_inlines(text + text_start, text_end - text_start);
					node->text = arena_strndup(text + url_start, url_end - url_start);
					*tail = node;
					tail = &node->next;
					i = url_end + 1;
					continue;
				}
			}
		}

		// Image: ![alt](url)
		if (c == '!' && i + 1 < len && text[i + 1] == '[') {
			size_t alt_start = i + 2;
			size_t alt_end = alt_start;

			while (alt_end < len && text[alt_end] != ']')
				alt_end++;

			if (alt_end < len && alt_end + 1 < len && text[alt_end + 1] == '(') {
				size_t url_start = alt_end + 2;
				size_t url_end = url_start;

				while (url_end < len && text[url_end] != ')')
					url_end++;

				if (url_end < len) {
					if (buf.len > 0) {
						struct inline_node *node = inline_new(INLINE_TEXT);
						node->text = buf_finish(&buf);
						*tail = node;
						tail = &node->next;
						buf_init(&buf);
					}

					struct inline_node *node = inline_new(INLINE_IMAGE);
					node->title = arena_strndup(text + alt_start, alt_end - alt_start);
					node->text = arena_strndup(text + url_start, url_end - url_start);
					*tail = node;
					tail = &node->next;
					i = url_end + 1;
					continue;
				}
			}
		}

		// Backslash escape
		if (c == '\\' && i + 1 < len) {
			buf_putc(&buf, text[i + 1]);
			i += 2;
			continue;
		}

		// Hard line break: two spaces before newline
		if (c == ' ' && i + 1 < len && text[i + 1] == ' ') {
			size_t j = i + 2;
			while (j < len && text[j] == ' ')
				j++;
			if (j < len && text[j] == '\n') {
				if (buf.len > 0) {
					struct inline_node *node = inline_new(INLINE_TEXT);
					node->text = buf_finish(&buf);
					*tail = node;
					tail = &node->next;
					buf_init(&buf);
				}

				struct inline_node *node = inline_new(INLINE_LINEBREAK);
				*tail = node;
				tail = &node->next;
				i = j + 1;
				continue;
			}
		}

		// Regular character
		buf_putc(&buf, c);
		i++;
	}

	// Flush remaining text
	if (buf.len > 0) {
		struct inline_node *node = inline_new(INLINE_TEXT);
		node->text = buf_finish(&buf);
		*tail = node;
	} else {
		buf_free(&buf);
	}

	scratch_reset(mark);

	return head;
}

// Parse ATX heading (# Heading)
static struct block_node *md_parse_atx_heading(struct md_parser *p)
{
	size_t level = md_count_char(p, '#');
	if (level < 1 || level > 6) return NULL;

	md_skip(p, level);
	md_skip_spaces(p);

	char *text = md_get_line(p);

	// Trim trailing # characters
	size_t len = strlen(text);
	while (len > 0 && text[len - 1] == '#')
		len--;
	while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t'))
		len--;
	text[len] = '\0';

	struct block_node *node = block_new(BLOCK_HEADING);
	node->heading.level = (int)level;
	node->inlines = md_parse_inlines(text, len);

	return node;
}

// Parse fenced code block (``` or ~~~)
static struct block_node *md_parse_fenced_code(struct md_parser *p, char fence_char)
{
	size_t fence_len = md_count_char(p, fence_char);
	if (fence_len < 3) return NULL;

	md_skip(p, fence_len);
	md_skip_spaces(p);

	// Get language (rest of line)
	char *language = md_get_line(p);
	size_t lang_len = strlen(language);
	while (lang_len > 0 && (language[lang_len - 1] == ' ' || language[lang_len - 1] == '\t'))
		lang_len--;
	language[lang_len] = '\0';

	// Collect code content
	struct buffer code;
	buf_init(&code);

	while (!md_eof(p)) {
		// Check for closing fence
		size_t close_len = md_count_char(p, fence_char);
		if (close_len >= fence_len) {
			size_t save_pos = p->pos;
			md_skip(p, close_len);
			if (md_rest_is_blank(p)) {
				md_skip_line(p);
				break;
			}
			p->pos = save_pos;  // Restore if not a closing fence
		}

		char *line = md_get_line(p);
		buf_puts(&code, line);
		buf_putc(&code, '\n');
	}

	struct block_node *node = block_new(BLOCK_CODE_BLOCK);
	node->code_block.language = lang_len > 0 ? language : NULL;
	node->code_block.code = buf_finish(&code);

	return node;
}

// Parse blockquote (> ...)
static struct block_node *md_parse_blockquote(struct md_parser *p)
{
	struct buffer content;
	buf_init(&content);

	while (!md_eof(p)) {
		if (md_peek(p) != '>') break;
		md_next(p);  // Skip >
		if (md_peek(p) == ' ') md_next(p);  // Skip optional space

		char *line = md_get_line(p);
		buf_puts(&content, line);
		buf_putc(&content, '\n');
	}

	// Parse content as nested blocks
	char *inner = buf_finish(&content);
	struct md_parser inner_parser = { inner, 0, strlen(inner) };

	struct block_node *node = block_new(BLOCK_BLOCKQUOTE);
	node->children = md_parse_blocks(&inner_parser);

	return node;
}

// Parse list item
static struct block_node *md_parse_list_item(struct md_parser *p, size_t indent)
{
	struct buffer content;
	buf_init(&content);

	// First line (already past the marker)
	char *line = md_get_line(p);
	buf_puts(&content, line);
	buf_putc(&content, '\n');

	// Continuation lines (indented or blank)
	while (!md_eof(p)) {
		// Count leading spaces
		size_t spaces = 0;
		while (md_peek_at(p, spaces) == ' ')
			spaces++;

		// Blank line - include but check if list continues
		if (md_peek_at(p, spaces) == '\n') {
			buf_putc(&content, '\n');
			md_skip_line(p);
			continue;
		}

		// Check if properly indented continuation
		if (spaces >= indent) {
			md_skip(p, indent);
			line = md_get_line(p);
			buf_puts(&content, line);
			buf_putc(&content, '\n');
			continue;
		}

		// Not a continuation
		break;
	}

	char *inner = buf_finish(&content);
	struct md_parser inner_parser = { inner, 0, strlen(inner) };

	struct block_node *node = block_new(BLOCK_LIST_ITEM);
	node->children = md_parse_blocks(&inner_parser);

	return node;
}

// Parse unordered list (- or * or +)
static struct block_node *md_parse_unordered_list(struct md_parser *p)
{
	struct block_node *node = block_new(BLOCK_LIST);
	node->list.ordered = false;

	struct block_node **tail = &node->children;

	while (!md_eof(p)) {
		char marker = md_peek(p);
		if (marker != '-' && marker != '*' && marker != '+') break;
		if (md_peek_at(p, 1) != ' ') break;

		md_skip(p, 2);  // Skip marker and space

		struct block_node *item = md_parse_list_item(p, 2);
		*tail = item;
		tail = &item->next;
	}

	return node;
}

// Parse ordered list (1. 2. etc)
static struct block_node *md_parse_ordered_list(struct md_parser *p)
{
	struct block_node *node = block_new(BLOCK_LIST);
	node->list.ordered = true;

	// Get starting number
	size_t i = 0;
	while (isdigit(md_peek_at(p, i)))
		i++;
	char *num_str = arena_strndup(p->input + p->pos, i);
	long start = strtol(num_str, NULL, 10);
	node->list.start = (start > 0 && start <= INT_MAX) ? (int)start : 1;

	struct block_node **tail = &node->children;

	while (!md_eof(p)) {
		// Check for digit(s) followed by . and space
		i = 0;
		while (isdigit(md_peek_at(p, i)))
			i++;
		if (i == 0 || md_peek_at(p, i) != '.' || md_peek_at(p, i + 1) != ' ')
			break;

		md_skip(p, i + 2);  // Skip number, dot, and space

		struct block_node *item = md_parse_list_item(p, i + 2);
		*tail = item;
		tail = &item->next;
	}

	return node;
}

// Parse thematic break (---, ***, ___)
static struct block_node *md_parse_thematic_break(struct md_parser *p)
{
	char c = md_peek(p);
	size_t count = 0;

	while (!md_eof(p) && md_peek(p) != '\n') {
		char ch = md_next(p);
		if (ch == c) count++;
		else if (ch != ' ' && ch != '\t') return NULL;
	}
	if (md_peek(p) == '\n') md_next(p);

	if (count < 3) return NULL;

	return block_new(BLOCK_THEMATIC_BREAK);
}

// Parse paragraph (default block type)
static struct block_node *md_parse_paragraph(struct md_parser *p)
{
	struct buffer content;
	buf_init(&content);

	while (!md_eof(p)) {
		// Check for blank line
		if (md_peek(p) == '\n') {
			md_next(p);
			break;
		}

		// Check for block-level constructs that end a paragraph
		char c = md_peek(p);
		if (c == '#' || c == '>' || c == '`' || c == '~')
			break;
		if ((c == '-' || c == '*' || c == '_') && md_count_char(p, c) >= 3)
			break;
		if ((c == '-' || c == '*' || c == '+') && md_peek_at(p, 1) == ' ')
			break;
		if (isdigit(c)) {
			size_t i = 0;
			while (isdigit(md_peek_at(p, i))) i++;
			if (md_peek_at(p, i) == '.' && md_peek_at(p, i + 1) == ' ')
				break;
		}
		if (md_is_pipe_table_start(p))
			break;

		char *line = md_get_line(p);
		if (content.len > 0)
			buf_putc(&content, '\n');
		buf_puts(&content, line);
	}

	if (content.len == 0) {
		buf_free(&content);
		return NULL;
	}

	char *text = buf_finish(&content);
	struct block_node *node = block_new(BLOCK_PARAGRAPH);
	node->inlines = md_parse_inlines(text, strlen(text));

	return node;
}

// Parse all blocks
static struct block_node *md_parse_blocks(struct md_parser *p)
{
	struct block_node *head = NULL;
	struct block_node **tail = &head;

	while (!md_eof(p)) {
		// Skip blank lines
		while (md_peek(p) == '\n')
			md_next(p);
		if (md_eof(p)) break;

		struct block_node *node = NULL;
		char c = md_peek(p);

		// ATX heading
		if (c == '#') {
			node = md_parse_atx_heading(p);
		}
		// Fenced code block
		else if (c == '`' && md_count_char(p, '`') >= 3) {
			node = md_parse_fenced_code(p, '`');
		}
		else if (c == '~' && md_count_char(p, '~') >= 3) {
			node = md_parse_fenced_code(p, '~');
		}
		// Blockquote
		else if (c == '>') {
			node = md_parse_blockquote(p);
		}
		// Thematic break
		else if ((c == '-' || c == '*' || c == '_') && md_count_char(p, c) >= 3) {
			node = md_parse_thematic_break(p);
		}
		// Unordered list
		else if ((c == '-' || c == '*' || c == '+') && md_peek_at(p, 1) == ' ') {
			node = md_parse_unordered_list(p);
		}
		// Ordered list
			else if (isdigit(c)) {
				size_t i = 0;
				while (isdigit(md_peek_at(p, i))) i++;
				if (md_peek_at(p, i) == '.' && md_peek_at(p, i + 1) == ' ') {
					node = md_parse_ordered_list(p);
				} else {
					node = md_parse_paragraph(p);
				}
			}
			// Pipe table
			else if (md_is_pipe_table_start(p)) {
				node = md_parse_pipe_table(p);
			}
			// Paragraph (default)
			else {
				node = md_parse_paragraph(p);
			}

		if (node) {
			*tail = node;
			tail = &node->next;
		}
	}

	return head;
}

// Main markdown parsing entry point
static struct document *parse_markdown(const char *input, size_t len)
{
	struct md_parser parser = { input, 0, len };
	struct document *doc = arena_zalloc(sizeof(*doc));
	doc->blocks = md_parse_blocks(&parser);
	return doc;
}

// ============================================================================
// HTML Parser
// ============================================================================

struct html_parser {
	const char *input;
	size_t pos;
	size_t len;
};

static inline char html_peek(struct html_parser *p)
{
	return p->pos < p->len ? p->input[p->pos] : '\0';
}

static inline char html_next(struct html_parser *p)
{
	return p->pos < p->len ? p->input[p->pos++] : '\0';
}

static inline void html_skip(struct html_parser *p, size_t n)
{
	p->pos += n;
	if (p->pos > p->len) p->pos = p->len;
}

static inline bool html_eof(struct html_parser *p)
{
	return p->pos >= p->len;
}

// Skip whitespace
static void html_skip_ws(struct html_parser *p)
{
	while (!html_eof(p) && isspace(html_peek(p)))
		html_next(p);
}

// Check if at string (case-insensitive)
static bool html_at_str(struct html_parser *p, const char *s)
{
	size_t i = 0;
	while (s[i]) {
		if (p->pos + i >= p->len) return false;
		if (tolower(p->input[p->pos + i]) != tolower(s[i]))
			return false;
		i++;
	}
	return true;
}

// Read tag name
static char *html_read_tag_name(struct html_parser *p)
{
	size_t start = p->pos;
	while (!html_eof(p) && (isalnum(html_peek(p)) || html_peek(p) == '-'))
		html_next(p);
	return arena_strndup(p->input + start, p->pos - start);
}

// Check if character is valid in HTML5 attribute name
// Excludes: control chars (0x00-0x1F, 0x7F-0x9F), space, ", ', >, /, =
static inline bool html_is_attr_name_char(char c)
{
	unsigned char uc = (unsigned char)c;
	if (uc <= 0x1F || (uc >= 0x7F && uc <= 0x9F)) return false;
	switch (c) {
		case ' ': case '"': case '\'': case '>': case '/': case '=':
			return false;
	}
	return true;
}

// Forward declaration
static void html_decode_entity(struct html_parser *p, struct buffer *buf, bool in_attribute);

// Read attribute value (decodes full character references, passes short refs through)
static char *html_read_attr_value(struct html_parser *p)
{
	char quote = html_peek(p);
	if (quote != '"' && quote != '\'') {
		// Unquoted value - no entity decoding
		size_t start = p->pos;
		while (!html_eof(p) && !isspace(html_peek(p)) && html_peek(p) != '>')
			html_next(p);
		return arena_strndup(p->input + start, p->pos - start);
	}

	html_next(p);  // Skip opening quote
	struct buffer buf;
	buf_init(&buf);
	while (!html_eof(p) && html_peek(p) != quote) {
		if (html_peek(p) == '&') {
			html_decode_entity(p, &buf, true);  // in_attribute=true: short refs pass through
		} else {
			buf_putc(&buf, html_next(p));
		}
	}
	if (html_peek(p) == quote) html_next(p);
	return buf_finish(&buf);
}

// Skip to end of tag
static void html_skip_tag(struct html_parser *p)
{
	while (!html_eof(p) && html_peek(p) != '>')
		html_next(p);
	if (html_peek(p) == '>') html_next(p);
}

// Write UTF-8 encoded codepoint to buffer
static void buf_put_codepoint(struct buffer *buf, uint32_t code)
{
	if (code < 0x80) {
		buf_putc(buf, (char)code);
	} else if (code < 0x800) {
		buf_putc(buf, (char)(0xC0u | (code >> 6)));
		buf_putc(buf, (char)(0x80u | (code & 0x3Fu)));
	} else if (code < 0x10000) {
		buf_putc(buf, (char)(0xE0u | (code >> 12)));
		buf_putc(buf, (char)(0x80u | ((code >> 6) & 0x3Fu)));
		buf_putc(buf, (char)(0x80u | (code & 0x3Fu)));
	} else {
		buf_putc(buf, (char)(0xF0u | (code >> 18)));
		buf_putc(buf, (char)(0x80u | ((code >> 12) & 0x3Fu)));
		buf_putc(buf, (char)(0x80u | ((code >> 6) & 0x3Fu)));
		buf_putc(buf, (char)(0x80u | (code & 0x3Fu)));
	}
}

// Decode HTML entity
// in_attribute: if true, short character references are not decoded (passed through as plaintext)
static void html_decode_entity(struct html_parser *p, struct buffer *buf, bool in_attribute)
{
	html_next(p);  // Skip &

	if (html_peek(p) == '#') {
		// Numeric character reference
		html_next(p);
		uint32_t code = 0;
		if (html_peek(p) == 'x' || html_peek(p) == 'X') {
			html_next(p);
			while (isxdigit(html_peek(p)) && code <= 0x10FFFF) {
				char c = html_next(p);
				code = code * 16u + (uint32_t)(isdigit((unsigned char)c) ? (c - '0') : (tolower((unsigned char)c) - 'a' + 10));
			}
		} else {
			while (isdigit(html_peek(p)) && code <= 0x10FFFF)
				code = code * 10u + (uint32_t)(html_next(p) - '0');
		}
		if (html_peek(p) == ';') html_next(p);
		// Clamp to valid Unicode range
		if (code > 0x10FFFF) code = 0xFFFD;  // Replacement character
		buf_put_codepoint(buf, code);
	} else {
		// Named character reference
		size_t start = p->pos;
		enum ShortCharacterReference short_ref = ShortCharacterReference_Unknown;
		size_t short_ref_len = 0;

		// Accumulate name, checking for short references at each step
		while (isalnum(html_peek(p))) {
			html_next(p);
			size_t len = p->pos - start;
			enum ShortCharacterReference ref = lookup_short_character_reference(p->input + start, len);
			if (ref != ShortCharacterReference_Unknown) {
				short_ref = ref;
				short_ref_len = len;
			}
		}

		size_t len = p->pos - start;
		bool has_semicolon = (html_peek(p) == ';');
		if (has_semicolon) html_next(p);

		if (has_semicolon) {
			// Check full character reference first (requires semicolon)
			enum FullCharacterReference full_ref = lookup_full_character_reference(p->input + start, len);
			if (full_ref != FullCharacterReference_Unknown) {
				buf_put_codepoint(buf, full_character_reference_codepoints[full_ref]);
				return;
			}
		}

		// Fall back to short reference if found (not in attributes)
		if (!in_attribute && short_ref != ShortCharacterReference_Unknown) {
			buf_put_codepoint(buf, short_character_reference_codepoints[short_ref]);
			// Put back any extra characters we consumed beyond the short ref
			// Example: &notit; -> short ref is "not", put back "it;" for re-parsing
			size_t extra = len - short_ref_len;
			if (has_semicolon) extra++;  // Also put back the semicolon
			p->pos -= extra;
			return;
		}

		// Unknown entity, output as plain text
		buf_putc(buf, '&');
		buf_write(buf, p->input + start, len);
		if (has_semicolon) buf_putc(buf, ';');
	}
}

// Forward declaration
static struct block_node *html_parse_blocks(struct html_parser *p);
static struct inline_node *html_parse_inlines(struct html_parser *p, const char *end_tag);
static struct block_node *html_parse_table_children(struct html_parser *p);
static struct block_node *html_parse_table_row(struct html_parser *p, bool header_section);

// Parse inline HTML content
static struct inline_node *html_parse_inlines(struct html_parser *p, const char *end_tag)
{
	struct inline_node *head = NULL;
	struct inline_node **tail = &head;
	struct buffer text;
	buf_init(&text);

	while (!html_eof(p)) {
		// Check for end tag
		if (end_tag && html_peek(p) == '<' && html_at_str(p, "</")) {
			size_t save = p->pos;
			html_skip(p, 2);
			char *tag = html_read_tag_name(p);
			if (strcasecmp(tag, end_tag) == 0) {
				html_skip_tag(p);
				break;
			}
			p->pos = save;
		}

		// HTML tag
			if (html_peek(p) == '<') {
				html_next(p);

			// Flush text
			if (text.len > 0) {
				struct inline_node *node = inline_new(INLINE_TEXT);
				node->text = buf_finish(&text);
				*tail = node;
				tail = &node->next;
				buf_init(&text);
			}

					char *tag = html_read_tag_name(p);

				if (strcasecmp(tag, "b") == 0 || strcasecmp(tag, "strong") == 0) {
					html_skip_tag(p);
					struct inline_node *node = inline_new(INLINE_BOLD);
					node->children = html_parse_inlines(p, tag);
				*tail = node;
				tail = &node->next;
			}
			else if (strcasecmp(tag, "i") == 0 || strcasecmp(tag, "em") == 0) {
				html_skip_tag(p);
				struct inline_node *node = inline_new(INLINE_ITALIC);
				node->children = html_parse_inlines(p, tag);
				*tail = node;
				tail = &node->next;
			}
			else if (strcasecmp(tag, "code") == 0) {
				html_skip_tag(p);
				struct buffer code;
				buf_init(&code);
				while (!html_eof(p)) {
					if (html_peek(p) == '<' && html_at_str(p, "</code")) {
						html_skip(p, 6);
						html_skip_tag(p);
						break;
					}
					if (html_peek(p) == '&') {
						html_decode_entity(p, &code, false);
					} else {
						buf_putc(&code, html_next(p));
					}
				}
				struct inline_node *node = inline_new(INLINE_CODE);
				node->text = buf_finish(&code);
				*tail = node;
				tail = &node->next;
			}
			else if (strcasecmp(tag, "a") == 0) {
				// Parse href attribute
				char *href = NULL;
				html_skip_ws(p);
				while (!html_eof(p) && html_peek(p) != '>') {
					size_t attr_start = p->pos;
					while (!html_eof(p) && html_is_attr_name_char(html_peek(p)))
						html_next(p);
					size_t attr_len = p->pos - attr_start;
					html_skip_ws(p);
					if (html_peek(p) == '=') {
						html_next(p);
						html_skip_ws(p);
						char *val = html_read_attr_value(p);
						if (attr_len == 4 && strncasecmp(p->input + attr_start, "href", 4) == 0)
							href = val;
					}
					html_skip_ws(p);
				}
				html_skip_tag(p);

				struct inline_node *node = inline_new(INLINE_LINK);
				node->text = href;
				node->children = html_parse_inlines(p, "a");
				*tail = node;
				tail = &node->next;
			}
			else if (strcasecmp(tag, "img") == 0) {
				char *src = NULL;
				char *alt = NULL;
				html_skip_ws(p);
				while (!html_eof(p) && html_peek(p) != '>' && html_peek(p) != '/') {
					size_t attr_start = p->pos;
					while (!html_eof(p) && html_is_attr_name_char(html_peek(p)))
						html_next(p);
					size_t attr_len = p->pos - attr_start;
					html_skip_ws(p);
					if (html_peek(p) == '=') {
						html_next(p);
						html_skip_ws(p);
						char *val = html_read_attr_value(p);
						if (attr_len == 3 && strncasecmp(p->input + attr_start, "src", 3) == 0)
							src = val;
						else if (attr_len == 3 && strncasecmp(p->input + attr_start, "alt", 3) == 0)
							alt = val;
					}
					html_skip_ws(p);
				}
				html_skip_tag(p);

				struct inline_node *node = inline_new(INLINE_IMAGE);
				node->text = src;
				node->title = alt;
				*tail = node;
				tail = &node->next;
			}
			else if (strcasecmp(tag, "br") == 0) {
				html_skip_tag(p);
				struct inline_node *node = inline_new(INLINE_LINEBREAK);
				*tail = node;
				tail = &node->next;
			}
			else {
				// Unknown tag - skip
				html_skip_tag(p);
			}

		}
		// Entity
		else if (html_peek(p) == '&') {
			html_decode_entity(p, &text, false);
		}
		// Regular text
		else {
			buf_putc(&text, html_next(p));
		}
	}

	// Flush remaining text
	if (text.len > 0) {
		struct inline_node *node = inline_new(INLINE_TEXT);
		node->text = buf_finish(&text);
		*tail = node;
	} else {
		buf_free(&text);
	}

	return head;
}

static struct block_node *html_parse_table_row(struct html_parser *p, bool header_section)
{
	struct block_node *row = block_new(BLOCK_TABLE_ROW);
	row->table_row.is_header = header_section;

	html_skip_tag(p);  // End of <tr ...>

	struct block_node **cell_tail = &row->children;

	while (!html_eof(p)) {
		html_skip_ws(p);
		if (html_eof(p)) break;

		if (html_peek(p) != '<') {
			html_next(p);
			continue;
		}

		if (html_at_str(p, "</")) {
			html_skip(p, 2);
			char *close = html_read_tag_name(p);
			html_skip_tag(p);
			if (strcasecmp(close, "tr") == 0)
				break;
			continue;
		}

		html_next(p);  // Skip <
		char *tag = html_read_tag_name(p);

		if (strcasecmp(tag, "td") == 0 || strcasecmp(tag, "th") == 0) {
			bool is_th = (strcasecmp(tag, "th") == 0);
			if (is_th)
				row->table_row.is_header = true;

			int colspan = 1;
			int rowspan = 1;
			html_skip_ws(p);
			while (!html_eof(p) && html_peek(p) != '>') {
				if (html_peek(p) == '/') {
					html_next(p);
					html_skip_ws(p);
					continue;
				}

				size_t attr_start = p->pos;
				while (!html_eof(p) && html_is_attr_name_char(html_peek(p)))
					html_next(p);
				size_t attr_len = p->pos - attr_start;
				if (attr_len == 0) {
					html_next(p);
					continue;
				}
				html_skip_ws(p);
				if (html_peek(p) == '=') {
					html_next(p);
					html_skip_ws(p);
					char *val = html_read_attr_value(p);
					if (attr_len == 7 && strncasecmp(p->input + attr_start, "colspan", 7) == 0) {
						char *end;
						long n = strtol(val, &end, 10);
						if (end != val && *end == '\0' && n >= 1 && n <= INT_MAX)
							colspan = (int)n;
					} else if (attr_len == 7 && strncasecmp(p->input + attr_start, "rowspan", 7) == 0) {
						char *end;
						long n = strtol(val, &end, 10);
						if (end != val && *end == '\0' && n >= 1 && n <= INT_MAX)
							rowspan = (int)n;
					}
				}
				html_skip_ws(p);
			}

			html_skip_tag(p);

			struct block_node *cell = block_new(BLOCK_TABLE_CELL);
			cell->table_cell.align = ALIGN_DEFAULT;
			cell->table_cell.colspan = colspan;
			cell->table_cell.rowspan = rowspan;
			cell->inlines = html_parse_inlines(p, tag);

			*cell_tail = cell;
			cell_tail = &cell->next;
			continue;
		}

		// Unknown tag inside a row - skip tag and keep scanning
		html_skip_tag(p);
	}

	return row;
}

static struct block_node *html_parse_table_children(struct html_parser *p)
{
	struct block_node *head = NULL;
	struct block_node **tail = &head;
	bool in_header = false;

	while (!html_eof(p)) {
		html_skip_ws(p);
		if (html_eof(p)) break;

		if (html_peek(p) != '<') {
			html_next(p);
			continue;
		}

		if (html_at_str(p, "</")) {
			html_skip(p, 2);
			char *close = html_read_tag_name(p);
			html_skip_tag(p);

			if (strcasecmp(close, "table") == 0)
				break;
			if (strcasecmp(close, "thead") == 0) {
				in_header = false;
				continue;
			}
			continue;
		}

		html_next(p);  // Skip <
		char *tag = html_read_tag_name(p);

		if (strcasecmp(tag, "thead") == 0) {
			in_header = true;
			html_skip_tag(p);
			continue;
		}
		if (strcasecmp(tag, "tbody") == 0 || strcasecmp(tag, "tfoot") == 0) {
			in_header = false;
			html_skip_tag(p);
			continue;
		}
		if (strcasecmp(tag, "tr") == 0) {
			struct block_node *row = html_parse_table_row(p, in_header);
			*tail = row;
			tail = &row->next;
			continue;
		}

		html_skip_tag(p);
	}

	return head;
}

// Parse HTML blocks
static struct block_node *html_parse_blocks(struct html_parser *p)
{
	struct block_node *head = NULL;
	struct block_node **tail = &head;

	while (!html_eof(p)) {
		html_skip_ws(p);
		if (html_eof(p)) break;

		if (html_peek(p) != '<') {
			// Text outside tags - wrap in paragraph
			struct buffer text;
			buf_init(&text);
			while (!html_eof(p) && html_peek(p) != '<') {
				if (html_peek(p) == '&') {
					html_decode_entity(p, &text, false);
				} else {
					buf_putc(&text, html_next(p));
				}
			}

			// Trim whitespace
			char *t = buf_finish(&text);
			size_t len = strlen(t);
			while (len > 0 && isspace(t[len - 1])) len--;
			size_t start = 0;
			while (start < len && isspace(t[start])) start++;

			if (start < len) {
				struct block_node *node = block_new(BLOCK_PARAGRAPH);
				node->inlines = md_parse_inlines(t + start, len - start);
				*tail = node;
				tail = &node->next;
			}
			continue;
		}

		html_next(p);  // Skip <

		// Skip comments
		if (html_at_str(p, "!--")) {
			html_skip(p, 3);
			while (!html_eof(p) && !html_at_str(p, "-->"))
				html_next(p);
			html_skip(p, 3);
			continue;
		}

		// Skip doctype
		if (html_at_str(p, "!doctype") || html_at_str(p, "!DOCTYPE")) {
			html_skip_tag(p);
			continue;
		}

		// Closing tag - skip
		if (html_peek(p) == '/') {
			html_skip_tag(p);
			continue;
		}

				char *tag = html_read_tag_name(p);

			// Heading
			if (tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && tag[2] == '\0') {
				int level = tag[1] - '0';
				html_skip_tag(p);

			struct block_node *node = block_new(BLOCK_HEADING);
			node->heading.level = level;
			node->inlines = html_parse_inlines(p, tag);
			*tail = node;
			tail = &node->next;
		}
		// Paragraph
		else if (strcasecmp(tag, "p") == 0) {
			html_skip_tag(p);
			struct block_node *node = block_new(BLOCK_PARAGRAPH);
			node->inlines = html_parse_inlines(p, "p");
			*tail = node;
			tail = &node->next;
		}
		// Code block
		else if (strcasecmp(tag, "pre") == 0) {
			html_skip_tag(p);
			html_skip_ws(p);

			char *language = NULL;

			// Check for <code> inside
			if (html_peek(p) == '<' && html_at_str(p, "<code")) {
				html_skip(p, 5);
				// Check for language class
				html_skip_ws(p);
				while (!html_eof(p) && html_peek(p) != '>') {
					size_t attr_start = p->pos;
					while (!html_eof(p) && html_is_attr_name_char(html_peek(p)))
						html_next(p);
					size_t attr_len = p->pos - attr_start;
					html_skip_ws(p);
					if (html_peek(p) == '=') {
						html_next(p);
						html_skip_ws(p);
						char *val = html_read_attr_value(p);
						if (attr_len == 5 && strncasecmp(p->input + attr_start, "class", 5) == 0) {
							// Extract language from class like "language-c"
							if (strncmp(val, "language-", 9) == 0) {
								language = arena_strdup(val + 9);
							}
						}
					}
					html_skip_ws(p);
				}
				html_skip_tag(p);
			}

			struct buffer code;
			buf_init(&code);
			while (!html_eof(p)) {
				if (html_peek(p) == '<' && (html_at_str(p, "</code") || html_at_str(p, "</pre"))) {
					break;
				}
				if (html_peek(p) == '&') {
					html_decode_entity(p, &code, false);
				} else {
					buf_putc(&code, html_next(p));
				}
			}

			// Skip closing tags
			if (html_at_str(p, "</code")) {
				html_skip(p, 6);
				html_skip_tag(p);
			}
			if (html_at_str(p, "</pre")) {
				html_skip(p, 5);
				html_skip_tag(p);
			}

			struct block_node *node = block_new(BLOCK_CODE_BLOCK);
			node->code_block.language = language;
			node->code_block.code = buf_finish(&code);
			*tail = node;
			tail = &node->next;
		}
		// Blockquote
		else if (strcasecmp(tag, "blockquote") == 0) {
			html_skip_tag(p);

			// Find end of blockquote
			int depth = 1;
			size_t start = p->pos;
			while (!html_eof(p) && depth > 0) {
				if (html_at_str(p, "<blockquote")) {
					depth++;
					html_skip(p, 11);
				} else if (html_at_str(p, "</blockquote")) {
					depth--;
					if (depth == 0) break;
					html_skip(p, 12);
				} else {
					html_next(p);
				}
			}

			char *inner = arena_strndup(p->input + start, p->pos - start);
			html_skip(p, 12);  // </blockquote
			html_skip_tag(p);

			struct html_parser inner_parser = { inner, 0, strlen(inner) };
			struct block_node *node = block_new(BLOCK_BLOCKQUOTE);
			node->children = html_parse_blocks(&inner_parser);
			*tail = node;
			tail = &node->next;
		}
		// Unordered list
		else if (strcasecmp(tag, "ul") == 0) {
			html_skip_tag(p);

			struct block_node *node = block_new(BLOCK_LIST);
			node->list.ordered = false;
			struct block_node **item_tail = &node->children;

			while (!html_eof(p)) {
				html_skip_ws(p);
				if (html_at_str(p, "</ul")) {
					html_skip(p, 4);
					html_skip_tag(p);
					break;
				}
				if (html_at_str(p, "<li")) {
					html_skip(p, 3);
					html_skip_tag(p);

					// Find end of li
					int depth = 1;
					size_t start = p->pos;
					while (!html_eof(p) && depth > 0) {
						if (html_at_str(p, "<li")) {
							depth++;
							html_skip(p, 3);
						} else if (html_at_str(p, "</li")) {
							depth--;
							if (depth == 0) break;
							html_skip(p, 4);
						} else {
							html_next(p);
						}
					}

					char *inner = arena_strndup(p->input + start, p->pos - start);
					if (html_at_str(p, "</li")) {
						html_skip(p, 4);
						html_skip_tag(p);
					}

					struct html_parser inner_parser = { inner, 0, strlen(inner) };
					struct block_node *item = block_new(BLOCK_LIST_ITEM);
					item->children = html_parse_blocks(&inner_parser);
					*item_tail = item;
					item_tail = &item->next;
				} else {
					html_next(p);
				}
			}

			*tail = node;
			tail = &node->next;
		}
		// Ordered list
		else if (strcasecmp(tag, "ol") == 0) {
			// Check for start attribute
			int start_num = 1;
			html_skip_ws(p);
			while (!html_eof(p) && html_peek(p) != '>') {
				size_t attr_start = p->pos;
				while (!html_eof(p) && html_is_attr_name_char(html_peek(p)))
					html_next(p);
				size_t attr_len = p->pos - attr_start;
				html_skip_ws(p);
				if (html_peek(p) == '=') {
					html_next(p);
					html_skip_ws(p);
					char *val = html_read_attr_value(p);
					if (attr_len == 5 && strncasecmp(p->input + attr_start, "start", 5) == 0) {
						long n = strtol(val, NULL, 10);
						start_num = (n > 0 && n <= INT_MAX) ? (int)n : 1;
					}
				}
				html_skip_ws(p);
			}
			html_skip_tag(p);

			struct block_node *node = block_new(BLOCK_LIST);
			node->list.ordered = true;
			node->list.start = start_num;
			struct block_node **item_tail = &node->children;

			while (!html_eof(p)) {
				html_skip_ws(p);
				if (html_at_str(p, "</ol")) {
					html_skip(p, 4);
					html_skip_tag(p);
					break;
				}
				if (html_at_str(p, "<li")) {
					html_skip(p, 3);
					html_skip_tag(p);

					int depth = 1;
					size_t start = p->pos;
					while (!html_eof(p) && depth > 0) {
						if (html_at_str(p, "<li")) {
							depth++;
							html_skip(p, 3);
						} else if (html_at_str(p, "</li")) {
							depth--;
							if (depth == 0) break;
							html_skip(p, 4);
						} else {
							html_next(p);
						}
					}

					char *inner = arena_strndup(p->input + start, p->pos - start);
					if (html_at_str(p, "</li")) {
						html_skip(p, 4);
						html_skip_tag(p);
					}

					struct html_parser inner_parser = { inner, 0, strlen(inner) };
					struct block_node *item = block_new(BLOCK_LIST_ITEM);
					item->children = html_parse_blocks(&inner_parser);
					*item_tail = item;
					item_tail = &item->next;
				} else {
					html_next(p);
				}
			}

			*tail = node;
			tail = &node->next;
		}
			// Thematic break
			else if (strcasecmp(tag, "hr") == 0) {
			html_skip_tag(p);
			struct block_node *node = block_new(BLOCK_THEMATIC_BREAK);
				*tail = node;
				tail = &node->next;
			}
			// Table
			else if (strcasecmp(tag, "table") == 0) {
				html_skip_tag(p);
				struct block_node *node = block_new(BLOCK_TABLE);
				node->children = html_parse_table_children(p);
				*tail = node;
				tail = &node->next;
			}
			// Skip other tags (html, head, body, div, span, etc.)
			else if (strcasecmp(tag, "html") == 0 || strcasecmp(tag, "head") == 0 ||
				 strcasecmp(tag, "body") == 0 || strcasecmp(tag, "div") == 0 ||
				 strcasecmp(tag, "span") == 0 || strcasecmp(tag, "meta") == 0 ||
				 strcasecmp(tag, "link") == 0 || strcasecmp(tag, "script") == 0 ||
				 strcasecmp(tag, "style") == 0 || strcasecmp(tag, "title") == 0) {
			html_skip_tag(p);

			// Skip content of script/style
			if (strcasecmp(tag, "script") == 0) {
				while (!html_eof(p) && !html_at_str(p, "</script"))
					html_next(p);
				if (html_at_str(p, "</script")) {
					html_skip(p, 8);
					html_skip_tag(p);
				}
			} else if (strcasecmp(tag, "style") == 0) {
				while (!html_eof(p) && !html_at_str(p, "</style"))
					html_next(p);
				if (html_at_str(p, "</style")) {
					html_skip(p, 7);
					html_skip_tag(p);
				}
			}
		}
		else {
			html_skip_tag(p);
		}

	}

	return head;
}

static struct document *parse_html(const char *input, size_t len)
{
	struct html_parser parser = { input, 0, len };
	struct document *doc = arena_zalloc(sizeof(*doc));
	doc->blocks = html_parse_blocks(&parser);
	return doc;
}

// ============================================================================
// HTML Writer
// ============================================================================

static void html_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '<': buf_puts(buf, "&lt;"); break;
			case '>': buf_puts(buf, "&gt;"); break;
			case '&': buf_puts(buf, "&amp;"); break;
			case '"': buf_puts(buf, "&quot;"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_html_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				html_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "<strong>");
				write_html_inlines(buf, node->children);
				buf_puts(buf, "</strong>");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "<em>");
				write_html_inlines(buf, node->children);
				buf_puts(buf, "</em>");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<code>");
				html_escape(buf, node->text);
				buf_puts(buf, "</code>");
				break;
			case INLINE_LINK:
				buf_puts(buf, "<a href=\"");
				html_escape(buf, node->text);
				buf_puts(buf, "\">");
				write_html_inlines(buf, node->children);
				buf_puts(buf, "</a>");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "<img src=\"");
				html_escape(buf, node->text);
				buf_puts(buf, "\" alt=\"");
				if (node->title) html_escape(buf, node->title);
				buf_puts(buf, "\">");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<br>\n");
				break;
		}
		node = node->next;
	}
}

static void write_html_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "<p>");
				write_html_inlines(buf, node->inlines);
				buf_puts(buf, "</p>\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "<h%d>", node->heading.level);
				write_html_inlines(buf, node->inlines);
				buf_printf(buf, "</h%d>\n", node->heading.level);
				break;
			case BLOCK_CODE_BLOCK:
				if (node->code_block.language) {
					buf_printf(buf, "<pre><code class=\"language-%s\">", node->code_block.language);
				} else {
					buf_puts(buf, "<pre><code>");
				}
				html_escape(buf, node->code_block.code);
				buf_puts(buf, "</code></pre>\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "<blockquote>\n");
				write_html_blocks(buf, node->children);
				buf_puts(buf, "</blockquote>\n");
				break;
			case BLOCK_LIST:
				if (node->list.ordered) {
					if (node->list.start != 1) {
						buf_printf(buf, "<ol start=\"%d\">\n", node->list.start);
					} else {
						buf_puts(buf, "<ol>\n");
					}
				} else {
					buf_puts(buf, "<ul>\n");
				}
				write_html_blocks(buf, node->children);
				buf_puts(buf, node->list.ordered ? "</ol>\n" : "</ul>\n");
				break;
			case BLOCK_LIST_ITEM:
				buf_puts(buf, "<li>");
				// If single paragraph, don't wrap in extra tags
				if (node->children && node->children->type == BLOCK_PARAGRAPH && !node->children->next) {
					write_html_inlines(buf, node->children->inlines);
				} else {
					write_html_blocks(buf, node->children);
				}
				buf_puts(buf, "</li>\n");
				break;
				case BLOCK_THEMATIC_BREAK:
					buf_puts(buf, "<hr>\n");
					break;
				case BLOCK_TABLE:
					buf_puts(buf, "<table>\n");
					{
						struct block_node *row = node->children;
						bool in_thead = false;
						bool in_tbody = false;

						while (row) {
							if (row->type != BLOCK_TABLE_ROW) {
								row = row->next;
								continue;
							}

							bool is_header = row->table_row.is_header;
							if (is_header) {
								if (!in_thead) {
									if (in_tbody) { buf_puts(buf, "</tbody>\n"); in_tbody = false; }
									buf_puts(buf, "<thead>\n");
									in_thead = true;
								}
							} else {
								if (!in_tbody) {
									if (in_thead) { buf_puts(buf, "</thead>\n"); in_thead = false; }
									buf_puts(buf, "<tbody>\n");
									in_tbody = true;
								}
							}

							buf_puts(buf, "<tr>");
							struct block_node *cell = row->children;
							while (cell) {
								if (cell->type != BLOCK_TABLE_CELL) {
									cell = cell->next;
									continue;
								}

									const char *tag = is_header ? "th" : "td";
									buf_printf(buf, "<%s", tag);
									if (cell->table_cell.colspan > 1)
										buf_printf(buf, " colspan=\"%d\"", cell->table_cell.colspan);
									if (cell->table_cell.rowspan > 1)
										buf_printf(buf, " rowspan=\"%d\"", cell->table_cell.rowspan);
									if (cell->table_cell.align != ALIGN_DEFAULT) {
										const char *align =
											cell->table_cell.align == ALIGN_CENTER ? "center" :
											cell->table_cell.align == ALIGN_RIGHT ? "right" :
										"left";
									buf_printf(buf, " style=\"text-align:%s;\"", align);
								}
								buf_putc(buf, '>');
								write_html_inlines(buf, cell->inlines);
								buf_printf(buf, "</%s>", tag);
								cell = cell->next;
							}
							buf_puts(buf, "</tr>\n");
							row = row->next;
						}

						if (in_thead) buf_puts(buf, "</thead>\n");
						if (in_tbody) buf_puts(buf, "</tbody>\n");
					}
					buf_puts(buf, "</table>\n");
					break;
				case BLOCK_TABLE_ROW:
				case BLOCK_TABLE_CELL:
					break;
			}
			node = node->next;
		}
	}

static char *write_html(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n");
	if (doc->title) {
		buf_puts(&buf, "<title>");
		html_escape(&buf, doc->title);
		buf_puts(&buf, "</title>\n");
	}
	buf_puts(&buf, "</head>\n<body>\n");
	write_html_blocks(&buf, doc->blocks);
	buf_puts(&buf, "</body>\n</html>\n");

	return buf_finish_malloc(&buf);
}

// ============================================================================
// Markdown Writer
// ============================================================================

static void write_md_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "**");
				write_md_inlines(buf, node->children);
				buf_puts(buf, "**");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "*");
				write_md_inlines(buf, node->children);
				buf_puts(buf, "*");
				break;
			case INLINE_CODE:
				buf_putc(buf, '`');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '`');
				break;
			case INLINE_LINK:
				buf_putc(buf, '[');
				write_md_inlines(buf, node->children);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "![");
				if (node->title) buf_puts(buf, node->title);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "  \n");
				break;
		}
		node = node->next;
	}
}

static void write_md_table_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) {
					for (const char *p = node->text; *p; p++) {
						if (*p == '|') buf_puts(buf, "\\|");
						else if (*p == '\n') buf_putc(buf, ' ');
						else buf_putc(buf, *p);
					}
				}
				break;
			case INLINE_BOLD:
				buf_puts(buf, "**");
				write_md_table_inlines(buf, node->children);
				buf_puts(buf, "**");
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '*');
				write_md_table_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_CODE:
				buf_putc(buf, '`');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '`');
				break;
			case INLINE_LINK:
				buf_putc(buf, '[');
				write_md_table_inlines(buf, node->children);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "![");
				if (node->title) buf_puts(buf, node->title);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, ' ');
				break;
		}
		node = node->next;
	}
}

static void write_md_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		// Add indentation
		for (int i = 0; i < indent; i++)
			buf_puts(buf, "    ");

		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_md_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '#');
				buf_putc(buf, ' ');
				write_md_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "```");
				if (node->code_block.language)
					buf_puts(buf, node->code_block.language);
				buf_putc(buf, '\n');
				buf_puts(buf, node->code_block.code);
				buf_puts(buf, "```\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				// Write children with > prefix
				{
					struct buffer inner;
					buf_init(&inner);
					write_md_blocks(&inner, node->children, 0);
					char *content = buf_finish(&inner);

					char *line = content;
					while (*line) {
						buf_puts(buf, "> ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "    ");
						if (node->list.ordered) {
							buf_printf(buf, "%d. ", num++);
						} else {
							buf_puts(buf, "- ");
						}
						// Write item content
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_md_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_md_blocks(buf, item->children, indent + 1);
						}
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				// Handled by BLOCK_LIST
				break;
				case BLOCK_THEMATIC_BREAK:
					buf_puts(buf, "---\n\n");
					break;
				case BLOCK_TABLE:
					{
						struct block_node *header = NULL;
						bool has_spans = false;
						for (struct block_node *r = node->children; r; r = r->next) {
							if (r->type != BLOCK_TABLE_ROW) continue;
							if (!header)
								header = r;
							for (struct block_node *c = r->children; c; c = c->next) {
								if (c->type != BLOCK_TABLE_CELL) continue;
								if (c->table_cell.colspan > 1 || c->table_cell.rowspan > 1) {
									has_spans = true;
									break;
								}
							}
							if (has_spans)
								break;
						}
	
						if (!header) {
							buf_putc(buf, '\n');
							break;
						}
	
						// Markdown pipe tables can't represent rowspan/colspan.
						if (has_spans) {
							bool first_row = true;
							for (struct block_node *r = node->children; r; r = r->next) {
								if (r->type != BLOCK_TABLE_ROW) continue;
								if (!first_row) {
									for (int i = 0; i < indent; i++)
										buf_puts(buf, "    ");
								}
								first_row = false;
	
								bool first_cell = true;
								for (struct block_node *c = r->children; c; c = c->next) {
									if (c->type != BLOCK_TABLE_CELL) continue;
									if (!first_cell)
										buf_puts(buf, " | ");
									first_cell = false;
									write_md_table_inlines(buf, c->inlines);
								}
								buf_putc(buf, '\n');
							}
							buf_putc(buf, '\n');
							break;
						}
	
						// Determine max columns across rows
						size_t max_cols = 0;
						for (struct block_node *r = node->children; r; r = r->next) {
							if (r->type != BLOCK_TABLE_ROW) continue;
							size_t cols = 0;
							for (struct block_node *c = r->children; c; c = c->next)
								if (c->type == BLOCK_TABLE_CELL) cols++;
							if (cols > max_cols) max_cols = cols;
						}
						if (max_cols == 0) {
							buf_putc(buf, '\n');
							break;
						}
	
						// Header row
						buf_putc(buf, '|');
						{
							struct block_node *c = header->children;
							for (size_t col = 0; col < max_cols; col++) {
								while (c && c->type != BLOCK_TABLE_CELL) c = c->next;
								buf_putc(buf, ' ');
								if (c) {
									write_md_table_inlines(buf, c->inlines);
									c = c->next;
								}
								buf_putc(buf, ' ');
								buf_putc(buf, '|');
							}
						}
						buf_putc(buf, '\n');
	
						// Separator row (use header alignment if present)
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "    ");
						buf_putc(buf, '|');
						{
							struct block_node *c = header->children;
							for (size_t col = 0; col < max_cols; col++) {
								while (c && c->type != BLOCK_TABLE_CELL) c = c->next;
								int align = ALIGN_DEFAULT;
								if (c) {
									align = (int)c->table_cell.align;
									c = c->next;
								}
								if (align == ALIGN_LEFT) buf_puts(buf, " :--- |");
								else if (align == ALIGN_CENTER) buf_puts(buf, " :---: |");
								else if (align == ALIGN_RIGHT) buf_puts(buf, " ---: |");
								else buf_puts(buf, " --- |");
							}
						}
						buf_putc(buf, '\n');
	
						// Body rows
						for (struct block_node *r = node->children; r; r = r->next) {
							if (r == header) continue;
							if (r->type != BLOCK_TABLE_ROW) continue;
	
							for (int i = 0; i < indent; i++)
								buf_puts(buf, "    ");
							buf_putc(buf, '|');
	
							struct block_node *c = r->children;
							for (size_t col = 0; col < max_cols; col++) {
								while (c && c->type != BLOCK_TABLE_CELL) c = c->next;
								buf_putc(buf, ' ');
								if (c) {
									write_md_table_inlines(buf, c->inlines);
									c = c->next;
								}
								buf_putc(buf, ' ');
								buf_putc(buf, '|');
							}
							buf_putc(buf, '\n');
						}
	
						buf_putc(buf, '\n');
					}
					break;
				case BLOCK_TABLE_ROW:
				case BLOCK_TABLE_CELL:
					break;
			}
			node = node->next;
		}
	}

static char *write_markdown(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_md_blocks(&buf, doc->blocks, 0);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Plain Text Writer
// ============================================================================

static void write_text_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
			case INLINE_ITALIC:
				write_text_inlines(buf, node->children);
				break;
			case INLINE_CODE:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_LINK:
				write_text_inlines(buf, node->children);
				if (node->text) {
					buf_puts(buf, " (");
					buf_puts(buf, node->text);
					buf_putc(buf, ')');
				}
				break;
			case INLINE_IMAGE:
				if (node->title) {
					buf_puts(buf, "[Image: ");
					buf_puts(buf, node->title);
					buf_putc(buf, ']');
				}
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_text_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		for (int i = 0; i < indent; i++)
			buf_puts(buf, "    ");

		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_text_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				write_text_inlines(buf, node->inlines);
				buf_putc(buf, '\n');
				for (int i = 0; i < indent; i++)
					buf_puts(buf, "    ");
				{
					// Underline with = or -
					struct buffer tmp;
					buf_init(&tmp);
					write_text_inlines(&tmp, node->inlines);
					char underline = (node->heading.level == 1) ? '=' : '-';
					for (size_t i = 0; i < tmp.len; i++)
						buf_putc(buf, underline);
					buf_free(&tmp);
				}
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				if (node->code_block.code) {
					char *line = node->code_block.code;
					while (*line) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "    ");
						buf_puts(buf, "    ");  // Extra indent for code
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_text_blocks(&inner, node->children, 0);
					char *line = inner.data;
					while (line && *line) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "    ");
						buf_puts(buf, "| ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "    ");
						if (node->list.ordered) {
							buf_printf(buf, "%d. ", num++);
						} else {
							buf_puts(buf, "* ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_text_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_text_blocks(buf, item->children, indent + 1);
						}
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				for (int i = 0; i < 40; i++)
					buf_putc(buf, '-');
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_text(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_text_blocks(&buf, doc->blocks, 0);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// RTF Writer
// ============================================================================

static void rtf_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		unsigned char c = (unsigned char)*s;
		if (c == '\\' || c == '{' || c == '}') {
			buf_putc(buf, '\\');
			buf_putc(buf, (char)c);
		} else if (c == '\n') {
			buf_puts(buf, "\\par\n");
		} else if (c >= 0x80) {
			// UTF-8 to RTF Unicode escape
			uint32_t code;
			if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
				code = ((uint32_t)(c & 0x1Fu) << 6) | (uint32_t)(s[1] & 0x3F);
				s++;
			} else if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
				code = ((uint32_t)(c & 0x0Fu) << 12) |
					((uint32_t)(s[1] & 0x3F) << 6) |
					(uint32_t)(s[2] & 0x3F);
				s += 2;
			} else if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
				code = ((uint32_t)(c & 0x07u) << 18) |
					((uint32_t)(s[1] & 0x3F) << 12) |
					((uint32_t)(s[2] & 0x3F) << 6) |
					(uint32_t)(s[3] & 0x3F);
				s += 3;
			} else {
				buf_putc(buf, '?');
				s++;
				continue;
			}
			buf_printf(buf, "\\u%d?", (int16_t)code);
		} else {
			buf_putc(buf, (char)c);
		}
		s++;
	}
}

static void write_rtf_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				rtf_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "{\\b ");
				write_rtf_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "{\\i ");
				write_rtf_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_CODE:
				buf_puts(buf, "{\\f1 ");
				rtf_escape(buf, node->text);
				buf_putc(buf, '}');
				break;
			case INLINE_LINK:
				buf_puts(buf, "{\\field{\\*\\fldinst HYPERLINK \"");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "\"}{\\fldrslt ");
				write_rtf_inlines(buf, node->children);
				buf_puts(buf, "}}");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[Image: ");
				if (node->title) rtf_escape(buf, node->title);
				buf_putc(buf, ']');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\line\n");
				break;
		}
		node = node->next;
	}
}

static void write_rtf_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "{\\pard ");
				write_rtf_inlines(buf, node->inlines);
				buf_puts(buf, "\\par}\n");
				break;
			case BLOCK_HEADING:
				{
					int size = 48 - (node->heading.level - 1) * 4;
					buf_printf(buf, "{\\pard\\fs%d\\b ", size);
					write_rtf_inlines(buf, node->inlines);
					buf_puts(buf, "\\par}\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "{\\pard\\f1\\fs20 ");
				rtf_escape(buf, node->code_block.code);
				buf_puts(buf, "\\par}\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "{\\pard\\li720 ");
				write_rtf_blocks(buf, node->children);
				buf_puts(buf, "\\par}\n");
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "{\\pard\\li720\\fi-360 ");
						if (node->list.ordered) {
							buf_printf(buf, "%d.\\tab ", num++);
						} else {
							buf_puts(buf, "\\bullet\\tab ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_rtf_inlines(buf, item->children->inlines);
						} else {
							write_rtf_blocks(buf, item->children);
						}
						buf_puts(buf, "\\par}\n");
						item = item->next;
					}
				}
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "{\\pard\\brdrb\\brdrs\\brdrw10\\brsp20 \\par}\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_rtf(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	// RTF header with font table
	buf_puts(&buf, "{\\rtf1\\ansi\\deff0\n");
	buf_puts(&buf, "{\\fonttbl{\\f0\\fswiss Helvetica;}{\\f1\\fmodern Courier New;}}\n");

	// Document info
	if (doc->title) {
		buf_puts(&buf, "{\\info{\\title ");
		rtf_escape(&buf, doc->title);
		buf_puts(&buf, "}}\n");
	}

	// Body
	write_rtf_blocks(&buf, doc->blocks);

	buf_puts(&buf, "}\n");

	return buf_finish_malloc(&buf);
}

// ============================================================================
// LaTeX Writer
// ============================================================================

static void latex_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '\\': buf_puts(buf, "\\textbackslash{}"); break;
			case '{': buf_puts(buf, "\\{"); break;
			case '}': buf_puts(buf, "\\}"); break;
			case '$': buf_puts(buf, "\\$"); break;
			case '&': buf_puts(buf, "\\&"); break;
			case '%': buf_puts(buf, "\\%"); break;
			case '#': buf_puts(buf, "\\#"); break;
			case '_': buf_puts(buf, "\\_"); break;
			case '^': buf_puts(buf, "\\^{}"); break;
			case '~': buf_puts(buf, "\\~{}"); break;
			case '<': buf_puts(buf, "\\textless{}"); break;
			case '>': buf_puts(buf, "\\textgreater{}"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_latex_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				latex_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "\\textbf{");
				write_latex_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "\\textit{");
				write_latex_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_CODE:
				buf_puts(buf, "\\texttt{");
				latex_escape(buf, node->text);
				buf_putc(buf, '}');
				break;
			case INLINE_LINK:
				buf_puts(buf, "\\href{");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "}{");
				write_latex_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "\\includegraphics{");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '}');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\\\\n");
				break;
		}
		node = node->next;
	}
}

static void write_latex_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_latex_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				{
					const char *cmd;
					switch (node->heading.level) {
						case 1: cmd = "\\section{"; break;
						case 2: cmd = "\\subsection{"; break;
						case 3: cmd = "\\subsubsection{"; break;
						case 4: cmd = "\\paragraph{"; break;
						case 5: cmd = "\\subparagraph{"; break;
						default: cmd = "\\textbf{"; break;
					}
					buf_puts(buf, cmd);
					write_latex_inlines(buf, node->inlines);
					buf_puts(buf, "}\n\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "\\begin{verbatim}\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "\\end{verbatim}\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "\\begin{quote}\n");
				write_latex_blocks(buf, node->children);
				buf_puts(buf, "\\end{quote}\n\n");
				break;
			case BLOCK_LIST:
				if (node->list.ordered) {
					if (node->list.start != 1) {
						buf_puts(buf, "\\begin{enumerate}\n");
						buf_printf(buf, "\\setcounter{enumi}{%d}\n", node->list.start - 1);
					} else {
						buf_puts(buf, "\\begin{enumerate}\n");
					}
				} else {
					buf_puts(buf, "\\begin{itemize}\n");
				}
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "\\item ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_latex_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_latex_blocks(buf, item->children);
						}
						item = item->next;
					}
				}
				buf_puts(buf, node->list.ordered ? "\\end{enumerate}\n\n" : "\\end{itemize}\n\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "\\hrulefill\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_latex(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	// LaTeX preamble
	buf_puts(&buf, "\\documentclass{article}\n");
	buf_puts(&buf, "\\usepackage[utf8]{inputenc}\n");
	buf_puts(&buf, "\\usepackage{hyperref}\n");
	buf_puts(&buf, "\\usepackage{graphicx}\n");
	if (doc->title) {
		buf_puts(&buf, "\\title{");
		latex_escape(&buf, doc->title);
		buf_puts(&buf, "}\n");
	}
	buf_puts(&buf, "\\begin{document}\n");
	if (doc->title) {
		buf_puts(&buf, "\\maketitle\n");
	}

	write_latex_blocks(&buf, doc->blocks);

	buf_puts(&buf, "\\end{document}\n");

	return buf_finish_malloc(&buf);
}

// ============================================================================
// JSON Writer
// ============================================================================

static void json_escape(struct buffer *buf, const char *s)
{
	if (!s) {
		buf_puts(buf, "null");
		return;
	}
	buf_putc(buf, '"');
	while (*s) {
		switch (*s) {
			case '"': buf_puts(buf, "\\\""); break;
			case '\\': buf_puts(buf, "\\\\"); break;
			case '\b': buf_puts(buf, "\\b"); break;
			case '\f': buf_puts(buf, "\\f"); break;
			case '\n': buf_puts(buf, "\\n"); break;
			case '\r': buf_puts(buf, "\\r"); break;
			case '\t': buf_puts(buf, "\\t"); break;
			default:
				if ((unsigned char)*s < 0x20) {
					buf_printf(buf, "\\u%04x", (unsigned char)*s);
				} else {
					buf_putc(buf, *s);
				}
		}
		s++;
	}
	buf_putc(buf, '"');
}

static void write_json_inlines(struct buffer *buf, struct inline_node *node, int depth)
{
	buf_puts(buf, "[\n");
	bool first = true;
	while (node) {
		if (!first) buf_puts(buf, ",\n");
		first = false;

		for (int i = 0; i < depth + 1; i++) buf_puts(buf, "  ");
		buf_puts(buf, "{");

		const char *type_str;
		switch (node->type) {
			case INLINE_TEXT: type_str = "text"; break;
			case INLINE_BOLD: type_str = "bold"; break;
			case INLINE_ITALIC: type_str = "italic"; break;
			case INLINE_CODE: type_str = "code"; break;
			case INLINE_LINK: type_str = "link"; break;
			case INLINE_IMAGE: type_str = "image"; break;
			case INLINE_LINEBREAK: type_str = "linebreak"; break;
			default: type_str = "unknown";
		}
		buf_puts(buf, "\"type\": ");
		json_escape(buf, type_str);

		if (node->text) {
			buf_puts(buf, ", \"text\": ");
			json_escape(buf, node->text);
		}
		if (node->title) {
			buf_puts(buf, ", \"title\": ");
			json_escape(buf, node->title);
		}
		if (node->children) {
			buf_puts(buf, ", \"children\": ");
			write_json_inlines(buf, node->children, depth + 1);
		}

		buf_putc(buf, '}');
		node = node->next;
	}
	buf_putc(buf, '\n');
	for (int i = 0; i < depth; i++) buf_puts(buf, "  ");
	buf_putc(buf, ']');
}

static void write_json_blocks(struct buffer *buf, struct block_node *node, int depth)
{
	buf_puts(buf, "[\n");
	bool first = true;
	while (node) {
		if (!first) buf_puts(buf, ",\n");
		first = false;

		for (int i = 0; i < depth + 1; i++) buf_puts(buf, "  ");
		buf_puts(buf, "{\n");

		const char *type_str;
		switch (node->type) {
			case BLOCK_PARAGRAPH: type_str = "paragraph"; break;
			case BLOCK_HEADING: type_str = "heading"; break;
			case BLOCK_CODE_BLOCK: type_str = "code_block"; break;
			case BLOCK_BLOCKQUOTE: type_str = "blockquote"; break;
			case BLOCK_LIST: type_str = "list"; break;
			case BLOCK_LIST_ITEM: type_str = "list_item"; break;
			case BLOCK_THEMATIC_BREAK: type_str = "thematic_break"; break;
			case BLOCK_TABLE: type_str = "table"; break;
			case BLOCK_TABLE_ROW: type_str = "table_row"; break;
			case BLOCK_TABLE_CELL: type_str = "table_cell"; break;
			default: type_str = "unknown";
		}

		for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
		buf_puts(buf, "\"type\": ");
		json_escape(buf, type_str);

		if (node->type == BLOCK_HEADING) {
			buf_printf(buf, ",\n");
			for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
			buf_printf(buf, "\"level\": %d", node->heading.level);
		}

		if (node->type == BLOCK_CODE_BLOCK) {
			if (node->code_block.language) {
				buf_puts(buf, ",\n");
				for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
				buf_puts(buf, "\"language\": ");
				json_escape(buf, node->code_block.language);
			}
			buf_puts(buf, ",\n");
			for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
			buf_puts(buf, "\"code\": ");
			json_escape(buf, node->code_block.code);
		}

		if (node->type == BLOCK_LIST) {
			buf_printf(buf, ",\n");
			for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
			buf_printf(buf, "\"ordered\": %s", node->list.ordered ? "true" : "false");
			if (node->list.ordered) {
				buf_printf(buf, ",\n");
				for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
				buf_printf(buf, "\"start\": %d", node->list.start);
			}
		}

		if (node->inlines) {
			buf_puts(buf, ",\n");
			for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
			buf_puts(buf, "\"inlines\": ");
			write_json_inlines(buf, node->inlines, depth + 2);
		}

		if (node->children) {
			buf_puts(buf, ",\n");
			for (int i = 0; i < depth + 2; i++) buf_puts(buf, "  ");
			buf_puts(buf, "\"children\": ");
			write_json_blocks(buf, node->children, depth + 2);
		}

		buf_putc(buf, '\n');
		for (int i = 0; i < depth + 1; i++) buf_puts(buf, "  ");
		buf_putc(buf, '}');
		node = node->next;
	}
	buf_putc(buf, '\n');
	for (int i = 0; i < depth; i++) buf_puts(buf, "  ");
	buf_putc(buf, ']');
}

static char *write_json(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "{\n");
	if (doc->title) {
		buf_puts(&buf, "  \"title\": ");
		json_escape(&buf, doc->title);
		buf_puts(&buf, ",\n");
	}
	buf_puts(&buf, "  \"blocks\": ");
	write_json_blocks(&buf, doc->blocks, 1);
	buf_puts(&buf, "\n}\n");

	return buf_finish_malloc(&buf);
}

// ============================================================================
// reStructuredText Writer
// ============================================================================

static void write_rst_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "**");
				write_rst_inlines(buf, node->children);
				buf_puts(buf, "**");
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '*');
				write_rst_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_CODE:
				buf_puts(buf, "``");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "``");
				break;
			case INLINE_LINK:
				buf_putc(buf, '`');
				write_rst_inlines(buf, node->children);
				buf_puts(buf, " <");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, ">`_");
				break;
			case INLINE_IMAGE:
				// Handled at block level
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_rst_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		for (int i = 0; i < indent; i++)
			buf_puts(buf, "   ");

		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_rst_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				{
					write_rst_inlines(buf, node->inlines);
					buf_putc(buf, '\n');

					// Calculate length of heading text
					struct buffer tmp;
					buf_init(&tmp);
					write_rst_inlines(&tmp, node->inlines);
					size_t len = tmp.len;
					buf_free(&tmp);

					for (int i = 0; i < indent; i++)
						buf_puts(buf, "   ");

					char underline;
					switch (node->heading.level) {
						case 1: underline = '='; break;
						case 2: underline = '-'; break;
						case 3: underline = '~'; break;
						case 4: underline = '^'; break;
						case 5: underline = '"'; break;
						default: underline = '\''; break;
					}
					for (size_t i = 0; i < len; i++)
						buf_putc(buf, underline);
					buf_puts(buf, "\n\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, ".. code-block::");
				if (node->code_block.language) {
					buf_putc(buf, ' ');
					buf_puts(buf, node->code_block.language);
				}
				buf_puts(buf, "\n\n");
				if (node->code_block.code) {
					char *line = node->code_block.code;
					while (*line) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "   ");
						buf_puts(buf, "   ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_rst_blocks(&inner, node->children, 0);
					char *line = inner.data;
					while (line && *line) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "   ");
						buf_puts(buf, "   ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "   ");
						if (node->list.ordered) {
							buf_printf(buf, "%d. ", num++);
						} else {
							buf_puts(buf, "* ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_rst_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_rst_blocks(buf, item->children, indent + 1);
						}
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_rst(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	if (doc->title) {
		size_t len = strlen(doc->title);
		for (size_t i = 0; i < len; i++)
			buf_putc(&buf, '=');
		buf_putc(&buf, '\n');
		buf_puts(&buf, doc->title);
		buf_putc(&buf, '\n');
		for (size_t i = 0; i < len; i++)
			buf_putc(&buf, '=');
		buf_puts(&buf, "\n\n");
	}

	write_rst_blocks(&buf, doc->blocks, 0);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// AsciiDoc Writer
// ============================================================================

static void write_asciidoc_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "*");
				write_asciidoc_inlines(buf, node->children);
				buf_puts(buf, "*");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "_");
				write_asciidoc_inlines(buf, node->children);
				buf_puts(buf, "_");
				break;
			case INLINE_CODE:
				buf_putc(buf, '`');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '`');
				break;
			case INLINE_LINK:
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '[');
				write_asciidoc_inlines(buf, node->children);
				buf_putc(buf, ']');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "image:");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '[');
				if (node->title) buf_puts(buf, node->title);
				buf_putc(buf, ']');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, " +\n");
				break;
		}
		node = node->next;
	}
}

static void write_asciidoc_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_asciidoc_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '=');
				buf_putc(buf, ' ');
				write_asciidoc_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				if (node->code_block.language) {
					buf_puts(buf, "[source,");
					buf_puts(buf, node->code_block.language);
					buf_puts(buf, "]\n");
				}
				buf_puts(buf, "----\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "[quote]\n____\n");
				write_asciidoc_blocks(buf, node->children, indent);
				buf_puts(buf, "____\n\n");
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_putc(buf, '*');
						if (node->list.ordered) {
							buf_puts(buf, ". ");
						} else {
							buf_puts(buf, "* ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_asciidoc_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_asciidoc_blocks(buf, item->children, indent + 1);
						}
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "'''\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_asciidoc(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	if (doc->title) {
		buf_puts(&buf, "= ");
		buf_puts(&buf, doc->title);
		buf_puts(&buf, "\n\n");
	}

	write_asciidoc_blocks(&buf, doc->blocks, 1);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Org Mode Writer
// ============================================================================

static void write_org_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_putc(buf, '*');
				write_org_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '/');
				write_org_inlines(buf, node->children);
				buf_putc(buf, '/');
				break;
			case INLINE_CODE:
				buf_putc(buf, '~');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '~');
				break;
			case INLINE_LINK:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "][");
				write_org_inlines(buf, node->children);
				buf_puts(buf, "]]");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "]]");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\\\\n");
				break;
		}
		node = node->next;
	}
}

static void write_org_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_org_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '*');
				buf_putc(buf, ' ');
				write_org_inlines(buf, node->inlines);
				buf_putc(buf, '\n');
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "#+BEGIN_SRC");
				if (node->code_block.language) {
					buf_putc(buf, ' ');
					buf_puts(buf, node->code_block.language);
				}
				buf_putc(buf, '\n');
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "#+END_SRC\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "#+BEGIN_QUOTE\n");
				write_org_blocks(buf, node->children, indent);
				buf_puts(buf, "#+END_QUOTE\n\n");
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "  ");
						if (node->list.ordered) {
							buf_printf(buf, "%d. ", num++);
						} else {
							buf_puts(buf, "- ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_org_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							buf_putc(buf, '\n');
							write_org_blocks(buf, item->children, indent + 1);
						}
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "-----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_org(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	if (doc->title) {
		buf_puts(&buf, "#+TITLE: ");
		buf_puts(&buf, doc->title);
		buf_puts(&buf, "\n\n");
	}

	write_org_blocks(&buf, doc->blocks, 0);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Textile Writer
// ============================================================================

static void write_textile_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "*");
				write_textile_inlines(buf, node->children);
				buf_puts(buf, "*");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "_");
				write_textile_inlines(buf, node->children);
				buf_puts(buf, "_");
				break;
			case INLINE_CODE:
				buf_puts(buf, "@");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "@");
				break;
			case INLINE_LINK:
				buf_putc(buf, '"');
				write_textile_inlines(buf, node->children);
				buf_puts(buf, "\":");
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_IMAGE:
				buf_putc(buf, '!');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '!');
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_textile_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "p. ");
				write_textile_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "h%d. ", node->heading.level);
				write_textile_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "bc. ");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "bq. ");
				write_textile_blocks(buf, node->children);
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, node->list.ordered ? "# " : "* ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_textile_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "---\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_textile(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_textile_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// MediaWiki Writer
// ============================================================================

static void write_mediawiki_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "'''");
				write_mediawiki_inlines(buf, node->children);
				buf_puts(buf, "'''");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "''");
				write_mediawiki_inlines(buf, node->children);
				buf_puts(buf, "''");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<code>");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "</code>");
				break;
			case INLINE_LINK:
				buf_putc(buf, '[');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ' ');
				write_mediawiki_inlines(buf, node->children);
				buf_putc(buf, ']');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[[File:");
				if (node->text) buf_puts(buf, node->text);
				if (node->title) {
					buf_puts(buf, "|alt=");
					buf_puts(buf, node->title);
				}
				buf_puts(buf, "]]");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<br/>\n");
				break;
		}
		node = node->next;
	}
}

static void write_mediawiki_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_mediawiki_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '=');
				buf_putc(buf, ' ');
				write_mediawiki_inlines(buf, node->inlines);
				buf_putc(buf, ' ');
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '=');
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<syntaxhighlight");
				if (node->code_block.language) {
					buf_puts(buf, " lang=\"");
					buf_puts(buf, node->code_block.language);
					buf_putc(buf, '"');
				}
				buf_puts(buf, ">\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "</syntaxhighlight>\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_mediawiki_blocks(&inner, node->children);
					char *line = inner.data;
					while (line && *line) {
						buf_putc(buf, ':');
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, node->list.ordered ? "# " : "* ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_mediawiki_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_mediawiki(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_mediawiki_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Creole Writer
// ============================================================================

static void write_creole_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "**");
				write_creole_inlines(buf, node->children);
				buf_puts(buf, "**");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "//");
				write_creole_inlines(buf, node->children);
				buf_puts(buf, "//");
				break;
			case INLINE_CODE:
				buf_puts(buf, "{{{");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "}}}");
				break;
			case INLINE_LINK:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '|');
				write_creole_inlines(buf, node->children);
				buf_puts(buf, "]]");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "{{");
				if (node->text) buf_puts(buf, node->text);
				if (node->title) {
					buf_putc(buf, '|');
					buf_puts(buf, node->title);
				}
				buf_puts(buf, "}}");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\\\\n");
				break;
		}
		node = node->next;
	}
}

static void write_creole_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_creole_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '=');
				buf_putc(buf, ' ');
				write_creole_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "{{{\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "}}}\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				write_creole_blocks(buf, node->children);
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, node->list.ordered ? "# " : "* ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_creole_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_creole(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_creole_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// DokuWiki Writer
// ============================================================================

static void write_dokuwiki_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "**");
				write_dokuwiki_inlines(buf, node->children);
				buf_puts(buf, "**");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "//");
				write_dokuwiki_inlines(buf, node->children);
				buf_puts(buf, "//");
				break;
			case INLINE_CODE:
				buf_puts(buf, "''");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "''");
				break;
			case INLINE_LINK:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '|');
				write_dokuwiki_inlines(buf, node->children);
				buf_puts(buf, "]]");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "{{");
				if (node->text) buf_puts(buf, node->text);
				if (node->title) {
					buf_putc(buf, '|');
					buf_puts(buf, node->title);
				}
				buf_puts(buf, "}}");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\\\\n");
				break;
		}
		node = node->next;
	}
}

static void write_dokuwiki_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_dokuwiki_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				{
					// DokuWiki uses inverse heading levels: ====== is h1, ===== is h2
					int marks = 7 - node->heading.level;
					if (marks < 2) marks = 2;
					for (int i = 0; i < marks; i++)
						buf_putc(buf, '=');
					buf_putc(buf, ' ');
					write_dokuwiki_inlines(buf, node->inlines);
					buf_putc(buf, ' ');
					for (int i = 0; i < marks; i++)
						buf_putc(buf, '=');
					buf_puts(buf, "\n\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<code");
				if (node->code_block.language) {
					buf_putc(buf, ' ');
					buf_puts(buf, node->code_block.language);
				}
				buf_puts(buf, ">\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "</code>\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_dokuwiki_blocks(&inner, node->children);
					char *line = inner.data;
					while (line && *line) {
						buf_putc(buf, '>');
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, node->list.ordered ? "  - " : "  * ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_dokuwiki_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_dokuwiki(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_dokuwiki_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Jira/Confluence Writer
// ============================================================================

static void write_jira_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_putc(buf, '*');
				write_jira_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '_');
				write_jira_inlines(buf, node->children);
				buf_putc(buf, '_');
				break;
			case INLINE_CODE:
				buf_puts(buf, "{{");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "}}");
				break;
			case INLINE_LINK:
				buf_putc(buf, '[');
				write_jira_inlines(buf, node->children);
				buf_putc(buf, '|');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ']');
				break;
			case INLINE_IMAGE:
				buf_putc(buf, '!');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '!');
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_jira_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_jira_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "h%d. ", node->heading.level);
				write_jira_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "{code");
				if (node->code_block.language) {
					buf_putc(buf, ':');
					buf_puts(buf, node->code_block.language);
				}
				buf_puts(buf, "}\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "{code}\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "{quote}\n");
				write_jira_blocks(buf, node->children);
				buf_puts(buf, "{quote}\n\n");
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, node->list.ordered ? "# " : "* ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_jira_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_jira(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_jira_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// BBCode Writer
// ============================================================================

static void write_bbcode_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "[b]");
				write_bbcode_inlines(buf, node->children);
				buf_puts(buf, "[/b]");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "[i]");
				write_bbcode_inlines(buf, node->children);
				buf_puts(buf, "[/i]");
				break;
			case INLINE_CODE:
				buf_puts(buf, "[code]");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "[/code]");
				break;
			case INLINE_LINK:
				buf_puts(buf, "[url=");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ']');
				write_bbcode_inlines(buf, node->children);
				buf_puts(buf, "[/url]");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[img]");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "[/img]");
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_bbcode_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_bbcode_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				{
					int size = 7 - node->heading.level;
					if (size < 1) size = 1;
					buf_printf(buf, "[size=%d][b]", size);
					write_bbcode_inlines(buf, node->inlines);
					buf_puts(buf, "[/b][/size]\n\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "[code]\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "[/code]\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "[quote]\n");
				write_bbcode_blocks(buf, node->children);
				buf_puts(buf, "[/quote]\n\n");
				break;
			case BLOCK_LIST:
				buf_puts(buf, node->list.ordered ? "[list=1]\n" : "[list]\n");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "[*]");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_bbcode_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_puts(buf, "[/list]\n\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "[hr]\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_bbcode(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_bbcode_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Gemtext Writer (Gemini Protocol)
// ============================================================================

static void write_gemtext_inlines(struct buffer *buf, struct inline_node *node)
{
	// Gemtext has no inline formatting, just output plain text
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
			case INLINE_ITALIC:
				write_gemtext_inlines(buf, node->children);
				break;
			case INLINE_CODE:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_LINK:
				write_gemtext_inlines(buf, node->children);
				break;
			case INLINE_IMAGE:
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_gemtext_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_gemtext_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				// Gemtext only supports 3 heading levels
				{
					int level = node->heading.level;
					if (level > 3) level = 3;
					for (int i = 0; i < level; i++)
						buf_putc(buf, '#');
					buf_putc(buf, ' ');
					write_gemtext_inlines(buf, node->inlines);
					buf_putc(buf, '\n');
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "```");
				if (node->code_block.language)
					buf_puts(buf, node->code_block.language);
				buf_putc(buf, '\n');
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "```\n");
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_gemtext_blocks(&inner, node->children);
					char *line = inner.data;
					while (line && *line) {
						buf_putc(buf, '>');
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "* ");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_gemtext_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "---\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}

	// Output links at the end (Gemtext style)
	// For now, inline links are just text; proper implementation would collect them
}

static char *write_gemtext(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_gemtext_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Djot Writer
// ============================================================================

static void write_djot_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_putc(buf, '*');
				write_djot_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '_');
				write_djot_inlines(buf, node->children);
				buf_putc(buf, '_');
				break;
			case INLINE_CODE:
				buf_putc(buf, '`');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '`');
				break;
			case INLINE_LINK:
				buf_putc(buf, '[');
				write_djot_inlines(buf, node->children);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "![");
				if (node->title) buf_puts(buf, node->title);
				buf_puts(buf, "](");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, ')');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\\\n");
				break;
		}
		node = node->next;
	}
}

static void write_djot_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_djot_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '#');
				buf_putc(buf, ' ');
				write_djot_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "```");
				if (node->code_block.language)
					buf_puts(buf, node->code_block.language);
				buf_putc(buf, '\n');
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "```\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_djot_blocks(&inner, node->children);
					char *line = inner.data;
					while (line && *line) {
						buf_puts(buf, "> ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						if (node->list.ordered) {
							buf_printf(buf, "%d. ", num++);
						} else {
							buf_puts(buf, "- ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_djot_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "* * *\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_djot(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);
	write_djot_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Man/Troff Writer
// ============================================================================

static void troff_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '\\': buf_puts(buf, "\\\\"); break;
			case '-': buf_puts(buf, "\\-"); break;
			case '.':
				// Escape leading dots
				if (buf->len == 0 || buf->data[buf->len - 1] == '\n')
					buf_puts(buf, "\\&");
				buf_putc(buf, '.');
				break;
			case '\'':
				if (buf->len == 0 || buf->data[buf->len - 1] == '\n')
					buf_puts(buf, "\\&");
				buf_putc(buf, '\'');
				break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_man_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				troff_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "\\fB");
				write_man_inlines(buf, node->children);
				buf_puts(buf, "\\fR");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "\\fI");
				write_man_inlines(buf, node->children);
				buf_puts(buf, "\\fR");
				break;
			case INLINE_CODE:
				buf_puts(buf, "\\fB");
				troff_escape(buf, node->text);
				buf_puts(buf, "\\fR");
				break;
			case INLINE_LINK:
				write_man_inlines(buf, node->children);
				if (node->text) {
					buf_puts(buf, " <");
					troff_escape(buf, node->text);
					buf_putc(buf, '>');
				}
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[IMAGE]");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "\n.br\n");
				break;
		}
		node = node->next;
	}
}

static void write_man_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, ".PP\n");
				write_man_inlines(buf, node->inlines);
				buf_putc(buf, '\n');
				break;
			case BLOCK_HEADING:
				if (node->heading.level <= 2) {
					buf_puts(buf, ".SH ");
				} else {
					buf_puts(buf, ".SS ");
				}
				write_man_inlines(buf, node->inlines);
				buf_putc(buf, '\n');
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, ".PP\n.nf\n.RS 4\n");
				troff_escape(buf, node->code_block.code);
				buf_puts(buf, "\n.RE\n.fi\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, ".RS 4\n");
				write_man_blocks(buf, node->children);
				buf_puts(buf, ".RE\n");
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						if (node->list.ordered) {
							buf_printf(buf, ".IP %d. 4\n", num++);
						} else {
							buf_puts(buf, ".IP \\(bu 4\n");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_man_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							write_man_blocks(buf, item->children);
						}
						item = item->next;
					}
				}
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, ".PP\n.ce\n* * *\n.ce 0\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_man(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	// Man page header
	buf_puts(&buf, ".TH ");
	if (doc->title) {
		buf_putc(&buf, '"');
		troff_escape(&buf, doc->title);
		buf_putc(&buf, '"');
	} else {
		buf_puts(&buf, "UNTITLED");
	}
	buf_puts(&buf, " 1\n");

	write_man_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Texinfo Writer
// ============================================================================

static void texinfo_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '@': buf_puts(buf, "@@"); break;
			case '{': buf_puts(buf, "@{"); break;
			case '}': buf_puts(buf, "@}"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_texinfo_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				texinfo_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "@strong{");
				write_texinfo_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "@emph{");
				write_texinfo_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_CODE:
				buf_puts(buf, "@code{");
				texinfo_escape(buf, node->text);
				buf_putc(buf, '}');
				break;
			case INLINE_LINK:
				buf_puts(buf, "@url{");
				if (node->text) texinfo_escape(buf, node->text);
				buf_puts(buf, ", ");
				write_texinfo_inlines(buf, node->children);
				buf_putc(buf, '}');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "@image{");
				if (node->text) texinfo_escape(buf, node->text);
				buf_putc(buf, '}');
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "@*\n");
				break;
		}
		node = node->next;
	}
}

static void write_texinfo_blocks(struct buffer *buf, struct block_node *node, int depth)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_texinfo_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				{
					const char *cmd;
					switch (node->heading.level + depth) {
						case 1: cmd = "@chapter "; break;
						case 2: cmd = "@section "; break;
						case 3: cmd = "@subsection "; break;
						case 4: cmd = "@subsubsection "; break;
						default: cmd = "@subsubheading "; break;
					}
					buf_puts(buf, cmd);
					write_texinfo_inlines(buf, node->inlines);
					buf_puts(buf, "\n\n");
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "@example\n");
				texinfo_escape(buf, node->code_block.code);
				buf_puts(buf, "@end example\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "@quotation\n");
				write_texinfo_blocks(buf, node->children, depth);
				buf_puts(buf, "@end quotation\n\n");
				break;
			case BLOCK_LIST:
				buf_puts(buf, node->list.ordered ? "@enumerate\n" : "@itemize @bullet\n");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "@item\n");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_texinfo_inlines(buf, item->children->inlines);
							buf_putc(buf, '\n');
						} else {
							write_texinfo_blocks(buf, item->children, depth);
						}
						item = item->next;
					}
				}
				buf_puts(buf, node->list.ordered ? "@end enumerate\n\n" : "@end itemize\n\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "@noindent\n@center * * *\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_texinfo(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "\\input texinfo\n");
	buf_puts(&buf, "@settitle ");
	if (doc->title) texinfo_escape(&buf, doc->title);
	else buf_puts(&buf, "Untitled");
	buf_puts(&buf, "\n\n");
	buf_puts(&buf, "@titlepage\n@title ");
	if (doc->title) texinfo_escape(&buf, doc->title);
	else buf_puts(&buf, "Untitled");
	buf_puts(&buf, "\n@end titlepage\n\n");
	buf_puts(&buf, "@contents\n\n");
	buf_puts(&buf, "@ifnottex\n@node Top\n@top ");
	if (doc->title) texinfo_escape(&buf, doc->title);
	else buf_puts(&buf, "Untitled");
	buf_puts(&buf, "\n@end ifnottex\n\n");

	write_texinfo_blocks(&buf, doc->blocks, 0);

	buf_puts(&buf, "@bye\n");
	return buf_finish_malloc(&buf);
}

// ============================================================================
// POD Writer (Perl Documentation)
// ============================================================================

static void write_pod_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "B<");
				write_pod_inlines(buf, node->children);
				buf_putc(buf, '>');
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "I<");
				write_pod_inlines(buf, node->children);
				buf_putc(buf, '>');
				break;
			case INLINE_CODE:
				buf_puts(buf, "C<");
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '>');
				break;
			case INLINE_LINK:
				buf_puts(buf, "L<");
				write_pod_inlines(buf, node->children);
				if (node->text) {
					buf_putc(buf, '|');
					buf_puts(buf, node->text);
				}
				buf_putc(buf, '>');
				break;
			case INLINE_IMAGE:
				// POD doesn't support images
				break;
			case INLINE_LINEBREAK:
				buf_putc(buf, '\n');
				break;
		}
		node = node->next;
	}
}

static void write_pod_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_pod_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "=head%d ", node->heading.level);
				write_pod_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				// POD uses indentation for code
				{
					const char *line = node->code_block.code;
					while (line && *line) {
						buf_puts(buf, "    ");
						const char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_BLOCKQUOTE:
				write_pod_blocks(buf, node->children);
				break;
			case BLOCK_LIST:
				buf_puts(buf, "=over 4\n\n");
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						if (node->list.ordered) {
							buf_printf(buf, "=item %d.\n\n", num++);
						} else {
							buf_puts(buf, "=item *\n\n");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_pod_inlines(buf, item->children->inlines);
							buf_puts(buf, "\n\n");
						} else {
							write_pod_blocks(buf, item->children);
						}
						item = item->next;
					}
				}
				buf_puts(buf, "=back\n\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "=for text\n---\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_pod(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "=pod\n\n");
	if (doc->title) {
		buf_puts(&buf, "=head1 NAME\n\n");
		buf_puts(&buf, doc->title);
		buf_puts(&buf, "\n\n");
	}

	write_pod_blocks(&buf, doc->blocks);

	buf_puts(&buf, "=cut\n");
	return buf_finish_malloc(&buf);
}

// ============================================================================
// DocBook XML Writer
// ============================================================================

static void docbook_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '<': buf_puts(buf, "&lt;"); break;
			case '>': buf_puts(buf, "&gt;"); break;
			case '&': buf_puts(buf, "&amp;"); break;
			case '"': buf_puts(buf, "&quot;"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_docbook_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				docbook_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "<emphasis role=\"bold\">");
				write_docbook_inlines(buf, node->children);
				buf_puts(buf, "</emphasis>");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "<emphasis>");
				write_docbook_inlines(buf, node->children);
				buf_puts(buf, "</emphasis>");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<literal>");
				docbook_escape(buf, node->text);
				buf_puts(buf, "</literal>");
				break;
			case INLINE_LINK:
				buf_puts(buf, "<link xlink:href=\"");
				if (node->text) docbook_escape(buf, node->text);
				buf_puts(buf, "\">");
				write_docbook_inlines(buf, node->children);
				buf_puts(buf, "</link>");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "<inlinemediaobject><imageobject><imagedata fileref=\"");
				if (node->text) docbook_escape(buf, node->text);
				buf_puts(buf, "\"/></imageobject></inlinemediaobject>");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<?linebreak?>");
				break;
		}
		node = node->next;
	}
}

static void write_docbook_blocks(struct buffer *buf, struct block_node *node, int depth)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "<para>");
				write_docbook_inlines(buf, node->inlines);
				buf_puts(buf, "</para>\n");
				break;
			case BLOCK_HEADING:
				{
					const char *tag;
					switch (node->heading.level + depth) {
						case 1: tag = "chapter"; break;
						case 2: tag = "section"; break;
						case 3: tag = "section"; break;
						default: tag = "section"; break;
					}
					buf_printf(buf, "<%s><title>", tag);
					write_docbook_inlines(buf, node->inlines);
					buf_printf(buf, "</title></%s>\n", tag);
				}
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<programlisting");
				if (node->code_block.language) {
					buf_puts(buf, " language=\"");
					docbook_escape(buf, node->code_block.language);
					buf_putc(buf, '"');
				}
				buf_putc(buf, '>');
				docbook_escape(buf, node->code_block.code);
				buf_puts(buf, "</programlisting>\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "<blockquote>\n");
				write_docbook_blocks(buf, node->children, depth);
				buf_puts(buf, "</blockquote>\n");
				break;
			case BLOCK_LIST:
				buf_puts(buf, node->list.ordered ? "<orderedlist>\n" : "<itemizedlist>\n");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "<listitem>\n");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							buf_puts(buf, "<para>");
							write_docbook_inlines(buf, item->children->inlines);
							buf_puts(buf, "</para>\n");
						} else {
							write_docbook_blocks(buf, item->children, depth);
						}
						buf_puts(buf, "</listitem>\n");
						item = item->next;
					}
				}
				buf_puts(buf, node->list.ordered ? "</orderedlist>\n" : "</itemizedlist>\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "<bridgehead renderas=\"other\">* * *</bridgehead>\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_docbook(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	buf_puts(&buf, "<article xmlns=\"http://docbook.org/ns/docbook\" ");
	buf_puts(&buf, "xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"5.0\">\n");
	if (doc->title) {
		buf_puts(&buf, "<title>");
		docbook_escape(&buf, doc->title);
		buf_puts(&buf, "</title>\n");
	}

	write_docbook_blocks(&buf, doc->blocks, 0);

	buf_puts(&buf, "</article>\n");
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Typst Writer
// ============================================================================

static void typst_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '#': buf_puts(buf, "\\#"); break;
			case '$': buf_puts(buf, "\\$"); break;
			case '*': buf_puts(buf, "\\*"); break;
			case '_': buf_puts(buf, "\\_"); break;
			case '@': buf_puts(buf, "\\@"); break;
			case '<': buf_puts(buf, "\\<"); break;
			case '>': buf_puts(buf, "\\>"); break;
			case '\\': buf_puts(buf, "\\\\"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_typst_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				typst_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_putc(buf, '*');
				write_typst_inlines(buf, node->children);
				buf_putc(buf, '*');
				break;
			case INLINE_ITALIC:
				buf_putc(buf, '_');
				write_typst_inlines(buf, node->children);
				buf_putc(buf, '_');
				break;
			case INLINE_CODE:
				buf_putc(buf, '`');
				if (node->text) buf_puts(buf, node->text);
				buf_putc(buf, '`');
				break;
			case INLINE_LINK:
				buf_puts(buf, "#link(\"");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "\")[");
				write_typst_inlines(buf, node->children);
				buf_putc(buf, ']');
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "#image(\"");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "\")");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, " \\\n");
				break;
		}
		node = node->next;
	}
}

static void write_typst_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_typst_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '=');
				buf_putc(buf, ' ');
				write_typst_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "```");
				if (node->code_block.language)
					buf_puts(buf, node->code_block.language);
				buf_putc(buf, '\n');
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "```\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "#quote[\n");
				write_typst_blocks(buf, node->children);
				buf_puts(buf, "]\n\n");
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						if (node->list.ordered) {
							buf_puts(buf, "+ ");
						} else {
							buf_puts(buf, "- ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_typst_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "#line(length: 100%)\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_typst(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	if (doc->title) {
		buf_puts(&buf, "#set document(title: \"");
		buf_puts(&buf, doc->title);
		buf_puts(&buf, "\")\n\n");
	}

	write_typst_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// OPML Writer (Outline Processor Markup Language)
// ============================================================================

static void opml_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '<': buf_puts(buf, "&lt;"); break;
			case '>': buf_puts(buf, "&gt;"); break;
			case '&': buf_puts(buf, "&amp;"); break;
			case '"': buf_puts(buf, "&quot;"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void write_opml_inlines_to_text(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				opml_escape(buf, node->text);
				break;
			case INLINE_BOLD:
			case INLINE_ITALIC:
				write_opml_inlines_to_text(buf, node->children);
				break;
			case INLINE_CODE:
				opml_escape(buf, node->text);
				break;
			case INLINE_LINK:
				write_opml_inlines_to_text(buf, node->children);
				break;
			case INLINE_IMAGE:
			case INLINE_LINEBREAK:
				break;
		}
		node = node->next;
	}
}

static void write_opml_blocks(struct buffer *buf, struct block_node *node, int indent)
{
	while (node) {
		for (int i = 0; i < indent; i++)
			buf_puts(buf, "  ");

		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "<outline text=\"");
				write_opml_inlines_to_text(buf, node->inlines);
				buf_puts(buf, "\"/>\n");
				break;
			case BLOCK_HEADING:
				buf_puts(buf, "<outline text=\"");
				write_opml_inlines_to_text(buf, node->inlines);
				buf_puts(buf, "\">\n");
				// Following blocks until next heading become children
				if (node->next) {
					struct block_node *child = node->next;
					while (child && child->type != BLOCK_HEADING) {
						write_opml_blocks(buf, child, indent + 1);
						node = child;
						child = child->next;
					}
				}
				for (int i = 0; i < indent; i++)
					buf_puts(buf, "  ");
				buf_puts(buf, "</outline>\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<outline text=\"[code]\" _note=\"");
				opml_escape(buf, node->code_block.code);
				buf_puts(buf, "\"/>\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "<outline text=\"[quote]\">\n");
				write_opml_blocks(buf, node->children, indent + 1);
				for (int i = 0; i < indent; i++)
					buf_puts(buf, "  ");
				buf_puts(buf, "</outline>\n");
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					while (item) {
						for (int i = 0; i < indent; i++)
							buf_puts(buf, "  ");
						buf_puts(buf, "<outline text=\"");
						if (item->children && item->children->type == BLOCK_PARAGRAPH) {
							write_opml_inlines_to_text(buf, item->children->inlines);
						}
						buf_puts(buf, "\"/>\n");
						item = item->next;
					}
				}
				break;
			case BLOCK_LIST_ITEM:
			case BLOCK_THEMATIC_BREAK:
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_opml(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	buf_puts(&buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	buf_puts(&buf, "<opml version=\"2.0\">\n");
	buf_puts(&buf, "  <head>\n");
	if (doc->title) {
		buf_puts(&buf, "    <title>");
		opml_escape(&buf, doc->title);
		buf_puts(&buf, "</title>\n");
	}
	buf_puts(&buf, "  </head>\n");
	buf_puts(&buf, "  <body>\n");

	write_opml_blocks(&buf, doc->blocks, 2);

	buf_puts(&buf, "  </body>\n");
	buf_puts(&buf, "</opml>\n");
	return buf_finish_malloc(&buf);
}

// ============================================================================
// Muse Writer (Emacs Muse)
// ============================================================================

static void write_muse_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				if (node->text) buf_puts(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "*");
				write_muse_inlines(buf, node->children);
				buf_puts(buf, "*");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "_");
				write_muse_inlines(buf, node->children);
				buf_puts(buf, "_");
				break;
			case INLINE_CODE:
				buf_puts(buf, "=");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "=");
				break;
			case INLINE_LINK:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "][");
				write_muse_inlines(buf, node->children);
				buf_puts(buf, "]]");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "[[");
				if (node->text) buf_puts(buf, node->text);
				buf_puts(buf, "]]");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<br>\n");
				break;
		}
		node = node->next;
	}
}

static void write_muse_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				write_muse_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_HEADING:
				for (int i = 0; i < node->heading.level; i++)
					buf_putc(buf, '*');
				buf_putc(buf, ' ');
				write_muse_inlines(buf, node->inlines);
				buf_puts(buf, "\n\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<example>\n");
				if (node->code_block.code) buf_puts(buf, node->code_block.code);
				buf_puts(buf, "</example>\n\n");
				break;
			case BLOCK_BLOCKQUOTE:
				{
					struct buffer inner;
					buf_init(&inner);
					write_muse_blocks(&inner, node->children);
					char *line = inner.data;
					while (line && *line) {
						buf_puts(buf, "  ");
						char *end = strchr(line, '\n');
						if (end) {
							buf_write(buf, line, (size_t)(end - line + 1));
							line = end + 1;
						} else {
							buf_puts(buf, line);
							buf_putc(buf, '\n');
							break;
						}
					}
					buf_free(&inner);
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST:
				{
					int num = node->list.start;
					struct block_node *item = node->children;
					while (item) {
						if (node->list.ordered) {
							buf_printf(buf, " %d. ", num++);
						} else {
							buf_puts(buf, " - ");
						}
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							write_muse_inlines(buf, item->children->inlines);
						}
						buf_putc(buf, '\n');
						item = item->next;
					}
				}
				buf_putc(buf, '\n');
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "----\n\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_muse(struct document *doc)
{
	struct buffer buf;
	buf_init_output(&buf);

	if (doc->title) {
		buf_puts(&buf, "#title ");
		buf_puts(&buf, doc->title);
		buf_puts(&buf, "\n\n");
	}

	write_muse_blocks(&buf, doc->blocks);
	return buf_finish_malloc(&buf);
}

// ============================================================================
// ZIP Handling (for DOCX/ODT/EPUB) - requires zlib
// ============================================================================

#ifdef HAVE_ZLIB

// ZIP structures
#pragma pack(push, 1)
struct zip_local_header {
	uint32_t signature;       // 0x04034b50
	uint16_t version;
	uint16_t flags;
	uint16_t compression;
	uint16_t mod_time;
	uint16_t mod_date;
	uint32_t crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t name_len;
	uint16_t extra_len;
};

struct zip_central_header {
	uint32_t signature;       // 0x02014b50
	uint16_t version_made;
	uint16_t version_needed;
	uint16_t flags;
	uint16_t compression;
	uint16_t mod_time;
	uint16_t mod_date;
	uint32_t crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t name_len;
	uint16_t extra_len;
	uint16_t comment_len;
	uint16_t disk_start;
	uint16_t internal_attr;
	uint32_t external_attr;
	uint32_t local_offset;
};

struct zip_end_record {
	uint32_t signature;       // 0x06054b50
	uint16_t disk_num;
	uint16_t disk_start;
	uint16_t num_entries_disk;
	uint16_t num_entries;
	uint32_t central_size;
	uint32_t central_offset;
	uint16_t comment_len;
};
#pragma pack(pop)

struct zip_entry {
	char *name;
	char *data;
	size_t size;
};

struct zip_archive {
	struct zip_entry *entries;
	size_t num_entries;
	size_t cap_entries;
};

static bool zip_read_struct_at(const char *data, size_t len, size_t pos, void *out, size_t out_size)
{
	if (pos > len || out_size > len - pos)
		return false;
	memcpy(out, data + pos, out_size);
	return true;
}

static struct zip_archive *zip_read(const char *data, size_t len)
{
	// Find end of central directory
	if (len < sizeof(struct zip_end_record))
		return NULL;

	static const unsigned char end_sig[4] = { 0x50, 0x4b, 0x05, 0x06 };
	size_t end_pos = len - sizeof(struct zip_end_record);
	bool found = false;
	for (;;) {
		if (memcmp(data + end_pos, end_sig, sizeof(end_sig)) == 0) {
			found = true;
			break;
		}
		if (end_pos == 0)
			break;
		end_pos--;
	}
	if (!found)
		return NULL;

	struct zip_end_record end;
	if (!zip_read_struct_at(data, len, end_pos, &end, sizeof(end)))
		return NULL;
	if (end.signature != 0x06054b50)
		return NULL;
	if (end.disk_num != 0 || end.disk_start != 0)
		return NULL;
	if (end.num_entries != end.num_entries_disk)
		return NULL;

	size_t central_pos = (size_t)end.central_offset;
	size_t central_size = (size_t)end.central_size;
	if (central_pos > len || central_size > len - central_pos)
		return NULL;
	size_t central_end = central_pos + central_size;
	if (central_end > end_pos)
		return NULL;

	size_t num_entries = (size_t)end.num_entries;
	if (num_entries > SIZE_MAX / sizeof(struct zip_entry))
		return NULL;

	struct zip_archive *zip = arena_zalloc(sizeof(*zip));
	zip->entries = arena_alloc(num_entries * sizeof(*zip->entries));
	zip->cap_entries = num_entries;

	// Read central directory
	for (size_t i = 0; i < num_entries && central_pos < central_end; i++) {
		struct zip_central_header central;
		if (!zip_read_struct_at(data, len, central_pos, &central, sizeof(central)))
			return NULL;
		if (central.signature != 0x02014b50)
			return NULL;

		size_t central_header_end = central_pos + sizeof(central);
		size_t central_name_end = central_header_end;
		if ((size_t)central.name_len > len - central_name_end)
			return NULL;
		central_name_end += central.name_len;

		size_t central_entry_end = central_name_end;
		if ((size_t)central.extra_len > len - central_entry_end)
			return NULL;
		central_entry_end += central.extra_len;
		if ((size_t)central.comment_len > len - central_entry_end)
			return NULL;
		central_entry_end += central.comment_len;
		if (central_entry_end > central_end)
			return NULL;

		// Read local file
		size_t local_pos = (size_t)central.local_offset;
		struct zip_local_header local;
		if (!zip_read_struct_at(data, len, local_pos, &local, sizeof(local)))
			return NULL;
		if (local.signature != 0x04034b50) {
			central_pos = central_entry_end;
			continue;
		}

		size_t data_pos = local_pos + sizeof(local);
		if ((size_t)local.name_len > len - data_pos)
			return NULL;
		data_pos += local.name_len;
		if ((size_t)local.extra_len > len - data_pos)
			return NULL;
		data_pos += local.extra_len;

		size_t compressed_size = (size_t)central.compressed_size;
		size_t file_size = (size_t)central.uncompressed_size;
		if (compressed_size > len - data_pos)
			return NULL;

		char *file_data = NULL;

		if (central.compression == 0) {
			// Stored
			if (compressed_size != file_size)
				return NULL;
			file_data = arena_strndup(data + data_pos, compressed_size);
		} else if (central.compression == 8) {
			// Deflate
			if (file_size == SIZE_MAX || compressed_size > UINT_MAX || file_size > UINT_MAX)
				return NULL;
			file_data = arena_alloc(file_size + 1);
			z_stream strm = {0};
			strm.next_in = (Bytef*)(data + data_pos);
			strm.avail_in = (uInt)compressed_size;
			strm.next_out = (Bytef*)file_data;
			strm.avail_out = (uInt)file_size;

			int zrc = inflateInit2(&strm, -MAX_WBITS);
			if (zrc != Z_OK)
				return NULL;

			do {
				zrc = inflate(&strm, Z_FINISH);
			} while (zrc == Z_OK);

			inflateEnd(&strm);

			if (zrc != Z_STREAM_END || strm.total_out != file_size || strm.total_in != compressed_size)
				return NULL;
			file_data[file_size] = '\0';
		}

		if (file_data) {
			zip->entries[zip->num_entries].name = arena_strndup(data + central_header_end, central.name_len);
			zip->entries[zip->num_entries].data = file_data;
			zip->entries[zip->num_entries].size = file_size;
			zip->num_entries++;
		}

		central_pos = central_entry_end;
	}

	return zip;
}

static char *zip_find(struct zip_archive *zip, const char *name, size_t *size)
{
	for (size_t i = 0; i < zip->num_entries; i++) {
		if (strcmp(zip->entries[i].name, name) == 0) {
			if (size) *size = zip->entries[i].size;
			return zip->entries[i].data;
		}
	}
	return NULL;
}

// CRC32 table
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void)
{
	if (crc32_table_init) return;
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (int j = 0; j < 8; j++) {
			c = (c >> 1) ^ ((c & 1) ? 0xEDB88320 : 0);
		}
		crc32_table[i] = c;
	}
	crc32_table_init = true;
}

static uint32_t calc_crc32(const char *data, size_t len)
{
	init_crc32_table();
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < len; i++) {
		crc = crc32_table[(crc ^ (uint8_t)data[i]) & 0xFF] ^ (crc >> 8);
	}
	return ~crc;
}

static char *zip_write(struct zip_archive *zip, size_t *out_len)
{
	struct buffer buf;
	buf_init(&buf);

	// Track offsets for central directory
	uint32_t *offsets = arena_alloc(zip->num_entries * sizeof(uint32_t));

	// Write local file headers and data
	for (size_t i = 0; i < zip->num_entries; i++) {
		offsets[i] = buf.len;

		struct zip_entry *entry = &zip->entries[i];
		size_t name_len = strlen(entry->name);

		// Compress data
		uLongf comp_len = compressBound(entry->size);
		char *comp_data = arena_alloc(comp_len);
		int comp_method = 8;  // Deflate

		z_stream strm = {0};
		strm.next_in = (Bytef*)entry->data;
		strm.avail_in = entry->size;
		strm.next_out = (Bytef*)comp_data;
		strm.avail_out = comp_len;

		if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
			deflate(&strm, Z_FINISH);
			comp_len = strm.total_out;
			deflateEnd(&strm);
		} else {
			// Fall back to stored
			comp_method = 0;
			memcpy(comp_data, entry->data, entry->size);
			comp_len = entry->size;
		}

		uint32_t crc = calc_crc32(entry->data, entry->size);

		struct zip_local_header local = {
			.signature = 0x04034b50,
			.version = 20,
			.flags = 0,
			.compression = comp_method,
			.mod_time = 0,
			.mod_date = 0,
			.crc32 = crc,
			.compressed_size = comp_len,
			.uncompressed_size = entry->size,
			.name_len = name_len,
			.extra_len = 0
		};

		buf_write(&buf, (char*)&local, sizeof(local));
		buf_write(&buf, entry->name, name_len);
		buf_write(&buf, comp_data, comp_len);
	}

	// Write central directory
	uint32_t central_offset = buf.len;

	for (size_t i = 0; i < zip->num_entries; i++) {
		struct zip_entry *entry = &zip->entries[i];
		size_t name_len = strlen(entry->name);
		uint32_t crc = calc_crc32(entry->data, entry->size);

		// Get compressed size from local header
		struct zip_local_header *local = (struct zip_local_header*)(buf.data + offsets[i]);

		struct zip_central_header central = {
			.signature = 0x02014b50,
			.version_made = 20,
			.version_needed = 20,
			.flags = 0,
			.compression = local->compression,
			.mod_time = 0,
			.mod_date = 0,
			.crc32 = crc,
			.compressed_size = local->compressed_size,
			.uncompressed_size = entry->size,
			.name_len = name_len,
			.extra_len = 0,
			.comment_len = 0,
			.disk_start = 0,
			.internal_attr = 0,
			.external_attr = 0,
			.local_offset = offsets[i]
		};

		buf_write(&buf, (char*)&central, sizeof(central));
		buf_write(&buf, entry->name, name_len);
	}

	uint32_t central_size = buf.len - central_offset;

	// Write end of central directory
	struct zip_end_record end = {
		.signature = 0x06054b50,
		.disk_num = 0,
		.disk_start = 0,
		.num_entries_disk = zip->num_entries,
		.num_entries = zip->num_entries,
		.central_size = central_size,
		.central_offset = central_offset,
		.comment_len = 0
	};

	buf_write(&buf, (char*)&end, sizeof(end));

	*out_len = buf.len;
	return buf.data;
}

static void zip_add(struct zip_archive *zip, const char *name, const char *data, size_t size)
{
	zip->entries[zip->num_entries].name = arena_strdup(name);
	zip->entries[zip->num_entries].data = arena_strndup(data, size);
	zip->entries[zip->num_entries].size = size;
	zip->num_entries++;
}

// ============================================================================
// DOCX Parser
// ============================================================================

// Parse DOCX inline content (w:r elements)
static struct inline_node *docx_parse_run(const char *xml, size_t len)
{
	struct inline_node *head = NULL;
	struct inline_node **tail = &head;

	const char *pos = xml;
	const char *end = xml + len;

	while (pos < end) {
		// Find <w:r> or <w:r ...>
		const char *run_start = strstr(pos, "<w:r");
		if (!run_start || run_start >= end) break;

		// Find end of this run
		const char *run_end = strstr(run_start, "</w:r>");
		if (!run_end || run_end >= end) break;
		run_end += 6;

		// Check for formatting in <w:rPr>
		bool is_bold = false;
		bool is_italic = false;

		const char *rpr = strstr(run_start, "<w:rPr>");
		if (rpr && rpr < run_end) {
			const char *bold = strstr(rpr, "<w:b");
			const char *italic = strstr(rpr, "<w:i");
			is_bold = bold != NULL && bold < run_end;
			is_italic = italic != NULL && italic < run_end;
		}

		// Get text content from <w:t>
		const char *t_start = strstr(run_start, "<w:t");
		if (t_start && t_start < run_end) {
			t_start = strchr(t_start, '>');
			if (t_start) {
				t_start++;
				const char *t_end = strstr(t_start, "</w:t>");
				if (t_end && t_end < run_end) {
					char *text = arena_strndup(t_start, t_end - t_start);

					struct inline_node *node;
					if (is_bold && is_italic) {
						// Nested: bold contains italic
						struct inline_node *text_node = inline_new(INLINE_TEXT);
						text_node->text = text;
						struct inline_node *italic = inline_new(INLINE_ITALIC);
						italic->children = text_node;
						node = inline_new(INLINE_BOLD);
						node->children = italic;
					} else if (is_bold) {
						struct inline_node *text_node = inline_new(INLINE_TEXT);
						text_node->text = text;
						node = inline_new(INLINE_BOLD);
						node->children = text_node;
					} else if (is_italic) {
						struct inline_node *text_node = inline_new(INLINE_TEXT);
						text_node->text = text;
						node = inline_new(INLINE_ITALIC);
						node->children = text_node;
					} else {
						node = inline_new(INLINE_TEXT);
						node->text = text;
					}

					*tail = node;
					tail = &node->next;
				}
			}
		}

		// Check for line break <w:br/>
		if (strstr(run_start, "<w:br") && strstr(run_start, "<w:br") < run_end) {
			struct inline_node *node = inline_new(INLINE_LINEBREAK);
			*tail = node;
			tail = &node->next;
		}

		pos = run_end;
	}

	return head;
}

// Parse DOCX paragraph
static struct block_node *docx_parse_paragraph(const char *xml, size_t len)
{
	// Check for heading style
	int heading_level = 0;
	const char *ppr = strstr(xml, "<w:pPr>");
	if (ppr && ppr < xml + len) {
		const char *style = strstr(ppr, "<w:pStyle");
		if (style && style < xml + len) {
			const char *val = strstr(style, "w:val=\"");
			if (val) {
				val += 7;
				if (strncmp(val, "Heading", 7) == 0) {
					heading_level = val[7] - '0';
					if (heading_level < 1 || heading_level > 6) heading_level = 0;
				}
			}
		}
	}

	struct block_node *node;
	if (heading_level > 0) {
		node = block_new(BLOCK_HEADING);
		node->heading.level = heading_level;
	} else {
		node = block_new(BLOCK_PARAGRAPH);
	}

	node->inlines = docx_parse_run(xml, len);
	return node;
}

static struct document *parse_docx(const char *data, size_t len)
{
	struct zip_archive *zip = zip_read(data, len);
	if (!zip) return NULL;

	// Find document.xml
	size_t doc_len;
	char *doc_xml = zip_find(zip, "word/document.xml", &doc_len);
	if (!doc_xml) {
				return NULL;
	}

	struct document *doc = arena_zalloc(sizeof(*doc));
	struct block_node **tail = &doc->blocks;

	// Find <w:body>
	char *body = strstr(doc_xml, "<w:body>");
	if (!body) {
				return doc;
	}
	body += 8;

	char *body_end = strstr(body, "</w:body>");
	if (!body_end) body_end = doc_xml + doc_len;

	// Parse paragraphs
	char *pos = body;
	while (pos < body_end) {
		char *p_start = strstr(pos, "<w:p");
		if (!p_start || p_start >= body_end) break;

		// Find matching </w:p>
		int depth = 1;
		char *p_end = p_start + 4;
		while (p_end < body_end && depth > 0) {
			if (strncmp(p_end, "<w:p", 4) == 0 && (p_end[4] == ' ' || p_end[4] == '>' || p_end[4] == '/')) {
				depth++;
				p_end += 4;
			} else if (strncmp(p_end, "</w:p>", 6) == 0) {
				depth--;
				if (depth == 0) break;
				p_end += 6;
			} else {
				p_end++;
			}
		}
		if (depth != 0 || p_end > body_end - 6)
			break;
		p_end += 6;  // Include </w:p>

		struct block_node *para = docx_parse_paragraph(p_start, p_end - p_start);
		if (para && para->inlines) {
			*tail = para;
			tail = &para->next;
		}
		// Unused nodes stay in arena but don't leak - arena is freed at exit

		pos = p_end;
	}

	return doc;
}

// ============================================================================
// DOCX Writer
// ============================================================================

static void docx_write_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		buf_puts(buf, "<w:r>");

		// Add formatting
		bool has_bold = node->type == INLINE_BOLD;
		bool has_italic = node->type == INLINE_ITALIC;

		if (has_bold || has_italic) {
			buf_puts(buf, "<w:rPr>");
			if (has_bold) buf_puts(buf, "<w:b/>");
			if (has_italic) buf_puts(buf, "<w:i/>");
			buf_puts(buf, "</w:rPr>");
		}

		switch (node->type) {
			case INLINE_TEXT:
				buf_puts(buf, "<w:t xml:space=\"preserve\">");
				// Escape XML
				for (const char *p = node->text; *p; p++) {
					switch (*p) {
						case '<': buf_puts(buf, "&lt;"); break;
						case '>': buf_puts(buf, "&gt;"); break;
						case '&': buf_puts(buf, "&amp;"); break;
						default: buf_putc(buf, *p);
					}
				}
				buf_puts(buf, "</w:t>");
				break;
			case INLINE_BOLD:
			case INLINE_ITALIC:
				buf_puts(buf, "</w:r>");
				docx_write_inlines(buf, node->children);
				buf_puts(buf, "<w:r>");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<w:t xml:space=\"preserve\">");
				for (const char *p = node->text; *p; p++) {
					switch (*p) {
						case '<': buf_puts(buf, "&lt;"); break;
						case '>': buf_puts(buf, "&gt;"); break;
						case '&': buf_puts(buf, "&amp;"); break;
						default: buf_putc(buf, *p);
					}
				}
				buf_puts(buf, "</w:t>");
				break;
			case INLINE_LINK:
				// DOCX links are complex, just output text for now
				buf_puts(buf, "</w:r>");
				docx_write_inlines(buf, node->children);
				buf_puts(buf, "<w:r>");
				break;
			case INLINE_IMAGE:
				// Images require relationships, skip for now
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<w:br/>");
				break;
		}

		buf_puts(buf, "</w:r>");
		node = node->next;
	}
}

static struct block_node *table_next_cell(struct block_node *node)
{
	while (node && node->type != BLOCK_TABLE_CELL)
		node = node->next;
	return node;
}

static size_t table_max_columns(struct block_node *table)
{
	size_t cap = 16;
	int *vstart = malloc(cap * sizeof(*vstart));
	int *vrows = calloc(cap, sizeof(*vrows));
	int *vspan = calloc(cap, sizeof(*vspan));
	if (!vstart || !vrows || !vspan) {
		free(vstart);
		free(vrows);
		free(vspan);
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	for (size_t i = 0; i < cap; i++)
		vstart[i] = -1;

	size_t max_cols = 0;

	for (struct block_node *row = table->children; row; row = row->next) {
		if (row->type != BLOCK_TABLE_ROW)
			continue;

		size_t max_active_end = 0;
		for (size_t i = 0; i < cap; i++) {
			if (vstart[i] == (int)i && vrows[i] > 0) {
				int span = vspan[i] > 0 ? vspan[i] : 1;
				size_t end = i + (size_t)span;
				if (end > max_active_end)
					max_active_end = end;
			}
		}

		size_t col = 0;
		struct block_node *cell = table_next_cell(row->children);
		while (cell || col < max_active_end) {
			if (col >= cap) {
				size_t new_cap = cap * 2;
				while (new_cap <= col) {
					if (new_cap > SIZE_MAX / 2) {
						new_cap = col + 1;
						break;
					}
					new_cap *= 2;
				}

				int *new_vstart = realloc(vstart, new_cap * sizeof(*new_vstart));
				int *new_vrows = realloc(vrows, new_cap * sizeof(*new_vrows));
				int *new_vspan = realloc(vspan, new_cap * sizeof(*new_vspan));
				if (!new_vstart || !new_vrows || !new_vspan) {
					free(new_vstart);
					free(new_vrows);
					free(new_vspan);
					free(vstart);
					free(vrows);
					free(vspan);
					PUTS_ERR("Error: out of memory\n");
					exit(1);
				}
				vstart = new_vstart;
				vrows = new_vrows;
				vspan = new_vspan;
				for (size_t i = cap; i < new_cap; i++) {
					vstart[i] = -1;
					vrows[i] = 0;
					vspan[i] = 0;
				}
				cap = new_cap;
			}

			if (vstart[col] != -1) {
				int start = vstart[col];
				if (start == (int)col && vrows[col] > 0) {
					int span = vspan[col] > 0 ? vspan[col] : 1;
					vrows[col]--;
					if (vrows[col] == 0) {
						for (int i = 0; i < span && col + (size_t)i < cap; i++) {
							if (vstart[col + (size_t)i] == (int)col)
								vstart[col + (size_t)i] = -1;
						}
						vspan[col] = 0;
					}
					col += (size_t)span;
				} else {
					col++;
				}
				continue;
			}

			if (cell) {
				int colspan = cell->table_cell.colspan > 0 ? cell->table_cell.colspan : 1;
				int rowspan = cell->table_cell.rowspan > 0 ? cell->table_cell.rowspan : 1;
				if (col + (size_t)colspan > cap) {
					size_t need = col + (size_t)colspan;
					size_t new_cap = cap;
					while (new_cap < need) {
						if (new_cap > SIZE_MAX / 2) {
							new_cap = need;
							break;
						}
						new_cap *= 2;
					}

					int *new_vstart = realloc(vstart, new_cap * sizeof(*new_vstart));
					int *new_vrows = realloc(vrows, new_cap * sizeof(*new_vrows));
					int *new_vspan = realloc(vspan, new_cap * sizeof(*new_vspan));
					if (!new_vstart || !new_vrows || !new_vspan) {
						free(new_vstart);
						free(new_vrows);
						free(new_vspan);
						free(vstart);
						free(vrows);
						free(vspan);
						PUTS_ERR("Error: out of memory\n");
						exit(1);
					}
					vstart = new_vstart;
					vrows = new_vrows;
					vspan = new_vspan;
					for (size_t i = cap; i < new_cap; i++) {
						vstart[i] = -1;
						vrows[i] = 0;
						vspan[i] = 0;
					}
					cap = new_cap;
				}

				if (rowspan > 1) {
					vrows[col] = rowspan - 1;
					vspan[col] = colspan;
					for (int i = 0; i < colspan; i++)
						vstart[col + (size_t)i] = (int)col;
				}
				col += (size_t)colspan;
				cell = table_next_cell(cell->next);
			} else {
				col++;
			}
		}

		if (col > max_cols)
			max_cols = col;
	}

	free(vstart);
	free(vrows);
	free(vspan);
	return max_cols;
}

static void docx_write_table(struct buffer *buf, struct block_node *table)
{
	size_t max_cols = table_max_columns(table);
	if (max_cols == 0) {
		buf_puts(buf, "</w:p><w:p>");
		return;
	}

	int *vstart = malloc(max_cols * sizeof(*vstart));
	int *vrows = calloc(max_cols, sizeof(*vrows));
	int *vspan = calloc(max_cols, sizeof(*vspan));
	if (!vstart || !vrows || !vspan) {
		free(vstart);
		free(vrows);
		free(vspan);
		PUTS_ERR("Error: out of memory\n");
		exit(1);
	}
	for (size_t i = 0; i < max_cols; i++)
		vstart[i] = -1;

	buf_puts(buf, "</w:p>");
	buf_puts(buf, "<w:tbl>");
	buf_puts(buf, "<w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/></w:tblPr>");

	for (struct block_node *row = table->children; row; row = row->next) {
		if (row->type != BLOCK_TABLE_ROW)
			continue;

		buf_puts(buf, "<w:tr>");
		if (row->table_row.is_header)
			buf_puts(buf, "<w:trPr><w:tblHeader/></w:trPr>");

		struct block_node *cell = table_next_cell(row->children);

		for (size_t col = 0; col < max_cols; col++) {
			if (vstart[col] != -1) {
				int start = vstart[col];
				if (start == (int)col && vrows[col] > 0) {
					int span = vspan[col] > 0 ? vspan[col] : 1;
					if ((size_t)span > max_cols - col)
						span = (int)(max_cols - col);

					buf_puts(buf, "<w:tc>");
					buf_puts(buf, "<w:tcPr>");
					if (span > 1)
						buf_printf(buf, "<w:gridSpan w:val=\"%d\"/>", span);
					buf_puts(buf, "<w:vMerge/>");
					buf_puts(buf, "</w:tcPr>");
					buf_puts(buf, "<w:p/>");
					buf_puts(buf, "</w:tc>");

					vrows[col]--;
					if (vrows[col] == 0) {
						for (int i = 0; i < span; i++) {
							if (vstart[col + (size_t)i] == (int)col)
								vstart[col + (size_t)i] = -1;
						}
						vspan[col] = 0;
					}

					col += (size_t)span - 1;
					continue;
				}
				continue;
			}

			if (cell) {
				int colspan = cell->table_cell.colspan > 0 ? cell->table_cell.colspan : 1;
				int rowspan = cell->table_cell.rowspan > 0 ? cell->table_cell.rowspan : 1;
				if ((size_t)colspan > max_cols - col)
					colspan = (int)(max_cols - col);

				buf_puts(buf, "<w:tc>");
				if (colspan > 1 || rowspan > 1) {
					buf_puts(buf, "<w:tcPr>");
					if (colspan > 1)
						buf_printf(buf, "<w:gridSpan w:val=\"%d\"/>", colspan);
					if (rowspan > 1)
						buf_puts(buf, "<w:vMerge w:val=\"restart\"/>");
					buf_puts(buf, "</w:tcPr>");
				}

				buf_puts(buf, "<w:p>");
				if (cell->table_cell.align != ALIGN_DEFAULT) {
					const char *jc =
						cell->table_cell.align == ALIGN_CENTER ? "center" :
						cell->table_cell.align == ALIGN_RIGHT ? "right" :
						"left";
					buf_printf(buf, "<w:pPr><w:jc w:val=\"%s\"/></w:pPr>", jc);
				}
				docx_write_inlines(buf, cell->inlines);
				buf_puts(buf, "</w:p>");
				buf_puts(buf, "</w:tc>");

				if (rowspan > 1) {
					vrows[col] = rowspan - 1;
					vspan[col] = colspan;
					for (int i = 0; i < colspan; i++)
						vstart[col + (size_t)i] = (int)col;
				}

				col += (size_t)colspan - 1;
				cell = table_next_cell(cell->next);
			} else {
				buf_puts(buf, "<w:tc><w:p/></w:tc>");
			}
		}

		buf_puts(buf, "</w:tr>");
	}

	buf_puts(buf, "</w:tbl>");
	buf_puts(buf, "<w:p>");

	free(vstart);
	free(vrows);
	free(vspan);
}

static void docx_write_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		buf_puts(buf, "<w:p>");

		switch (node->type) {
			case BLOCK_PARAGRAPH:
				docx_write_inlines(buf, node->inlines);
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "<w:pPr><w:pStyle w:val=\"Heading%d\"/></w:pPr>", node->heading.level);
				docx_write_inlines(buf, node->inlines);
				break;
			case BLOCK_CODE_BLOCK:
				// Use monospace style
				buf_puts(buf, "<w:pPr><w:pStyle w:val=\"Code\"/></w:pPr>");
				buf_puts(buf, "<w:r><w:t xml:space=\"preserve\">");
				for (const char *p = node->code_block.code; *p; p++) {
					switch (*p) {
						case '<': buf_puts(buf, "&lt;"); break;
						case '>': buf_puts(buf, "&gt;"); break;
						case '&': buf_puts(buf, "&amp;"); break;
						case '\n': buf_puts(buf, "</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val=\"Code\"/></w:pPr><w:r><w:t xml:space=\"preserve\">"); break;
						default: buf_putc(buf, *p);
					}
				}
				buf_puts(buf, "</w:t></w:r>");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "</w:p>");
				docx_write_blocks(buf, node->children);
				buf_puts(buf, "<w:p>");
				break;
			case BLOCK_LIST:
				buf_puts(buf, "</w:p>");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "<w:p>");
						buf_puts(buf, "<w:pPr><w:pStyle w:val=\"ListParagraph\"/></w:pPr>");
						if (item->children && item->children->type == BLOCK_PARAGRAPH) {
							docx_write_inlines(buf, item->children->inlines);
						}
						buf_puts(buf, "</w:p>");
						item = item->next;
					}
				}
				buf_puts(buf, "<w:p>");
				break;
				case BLOCK_LIST_ITEM:
					// Handled by BLOCK_LIST
					break;
				case BLOCK_THEMATIC_BREAK:
					// Insert horizontal line
					buf_puts(buf, "<w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:space=\"1\" w:color=\"auto\"/></w:pBdr></w:pPr>");
					break;
				case BLOCK_TABLE:
					docx_write_table(buf, node);
					break;
				case BLOCK_TABLE_ROW:
				case BLOCK_TABLE_CELL:
					break;
			}

		buf_puts(buf, "</w:p>");
		node = node->next;
	}
}

static char *write_docx(struct document *doc, size_t *out_len)
{
	struct zip_archive *zip = arena_zalloc(sizeof(*zip));
	zip->entries = arena_alloc(4 * sizeof(*zip->entries));
	zip->cap_entries = 4;

	// [Content_Types].xml
	const char *content_types =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
		"<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
		"</Types>";
	zip_add(zip, "[Content_Types].xml", content_types, strlen(content_types));

	// _rels/.rels
	const char *rels =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
		"</Relationships>";
	zip_add(zip, "_rels/.rels", rels, strlen(rels));

	// word/_rels/document.xml.rels
	const char *doc_rels =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		"</Relationships>";
	zip_add(zip, "word/_rels/document.xml.rels", doc_rels, strlen(doc_rels));

	// word/document.xml
	struct buffer doc_buf;
	buf_init(&doc_buf);
	buf_puts(&doc_buf,
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
		"<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
		"<w:body>");

	docx_write_blocks(&doc_buf, doc->blocks);

	buf_puts(&doc_buf, "</w:body></w:document>");

	char *doc_xml = buf_finish(&doc_buf);
	zip_add(zip, "word/document.xml", doc_xml, strlen(doc_xml));

	char *result = zip_write(zip, out_len);

	return result;
}

// ============================================================================
// ODT Writer (OpenDocument Text)
// ============================================================================

static void odt_xml_escape(struct buffer *buf, const char *s)
{
	if (!s) return;
	while (*s) {
		switch (*s) {
			case '<': buf_puts(buf, "&lt;"); break;
			case '>': buf_puts(buf, "&gt;"); break;
			case '&': buf_puts(buf, "&amp;"); break;
			case '"': buf_puts(buf, "&quot;"); break;
			case '\'': buf_puts(buf, "&apos;"); break;
			default: buf_putc(buf, *s);
		}
		s++;
	}
}

static void odt_write_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				buf_puts(buf, "<text:span>");
				odt_xml_escape(buf, node->text);
				buf_puts(buf, "</text:span>");
				break;
			case INLINE_BOLD:
				buf_puts(buf, "<text:span text:style-name=\"Bold\">");
				odt_write_inlines(buf, node->children);
				buf_puts(buf, "</text:span>");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "<text:span text:style-name=\"Italic\">");
				odt_write_inlines(buf, node->children);
				buf_puts(buf, "</text:span>");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<text:span text:style-name=\"Code\">");
				odt_xml_escape(buf, node->text);
				buf_puts(buf, "</text:span>");
				break;
			case INLINE_LINK:
				buf_puts(buf, "<text:a xlink:type=\"simple\" xlink:href=\"");
				if (node->text) odt_xml_escape(buf, node->text);
				buf_puts(buf, "\">");
				odt_write_inlines(buf, node->children);
				buf_puts(buf, "</text:a>");
				break;
			case INLINE_IMAGE:
				// Images in ODT require embedding - simplified
				buf_puts(buf, "<text:span>[Image: ");
				if (node->title) odt_xml_escape(buf, node->title);
				buf_puts(buf, "]</text:span>");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<text:line-break/>");
				break;
		}
		node = node->next;
	}
}

static void odt_write_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "<text:p text:style-name=\"Text_Body\">");
				odt_write_inlines(buf, node->inlines);
				buf_puts(buf, "</text:p>\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "<text:h text:style-name=\"Heading_%d\" text:outline-level=\"%d\">",
					node->heading.level, node->heading.level);
				odt_write_inlines(buf, node->inlines);
				buf_puts(buf, "</text:h>\n");
				break;
			case BLOCK_CODE_BLOCK:
				buf_puts(buf, "<text:p text:style-name=\"Preformatted_Text\">");
				if (node->code_block.code) {
					const char *p = node->code_block.code;
					while (*p) {
						if (*p == '\n') {
							buf_puts(buf, "</text:p>\n<text:p text:style-name=\"Preformatted_Text\">");
						} else if (*p == '<') {
							buf_puts(buf, "&lt;");
						} else if (*p == '>') {
							buf_puts(buf, "&gt;");
						} else if (*p == '&') {
							buf_puts(buf, "&amp;");
						} else {
							buf_putc(buf, *p);
						}
						p++;
					}
				}
				buf_puts(buf, "</text:p>\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "<text:p text:style-name=\"Quotations\">");
				odt_write_blocks(buf, node->children);
				buf_puts(buf, "</text:p>\n");
				break;
			case BLOCK_LIST:
				buf_puts(buf, node->list.ordered ?
					"<text:list text:style-name=\"Numbering_1\">\n" :
					"<text:list text:style-name=\"List_1\">\n");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "<text:list-item>\n");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							buf_puts(buf, "<text:p text:style-name=\"List_Contents\">");
							odt_write_inlines(buf, item->children->inlines);
							buf_puts(buf, "</text:p>\n");
						} else {
							odt_write_blocks(buf, item->children);
						}
						buf_puts(buf, "</text:list-item>\n");
						item = item->next;
					}
				}
				buf_puts(buf, "</text:list>\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "<text:p text:style-name=\"Horizontal_Line\"/>\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_odt(struct document *doc, size_t *out_len)
{
	struct zip_archive *zip = arena_zalloc(sizeof(*zip));
	zip->entries = arena_alloc(8 * sizeof(*zip->entries));
	zip->cap_entries = 8;

	// mimetype (must be first, uncompressed)
	const char *mimetype = "application/vnd.oasis.opendocument.text";
	zip_add(zip, "mimetype", mimetype, strlen(mimetype));

	// META-INF/manifest.xml
	const char *manifest =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n"
		"  <manifest:file-entry manifest:full-path=\"/\" manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>\n"
		"  <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
		"  <manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>\n"
		"</manifest:manifest>";
	zip_add(zip, "META-INF/manifest.xml", manifest, strlen(manifest));

	// styles.xml
	const char *styles =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<office:document-styles xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
		"xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
		"xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" "
		"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\">\n"
		"<office:styles>\n"
		"  <style:style style:name=\"Bold\" style:family=\"text\"><style:text-properties fo:font-weight=\"bold\"/></style:style>\n"
		"  <style:style style:name=\"Italic\" style:family=\"text\"><style:text-properties fo:font-style=\"italic\"/></style:style>\n"
		"  <style:style style:name=\"Code\" style:family=\"text\"><style:text-properties style:font-name=\"Courier New\"/></style:style>\n"
		"</office:styles>\n"
		"</office:document-styles>";
	zip_add(zip, "styles.xml", styles, strlen(styles));

	// content.xml
	struct buffer content;
	buf_init(&content);
	buf_puts(&content,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<office:document-content "
		"xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
		"xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
		"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
		"xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
		"<office:body>\n"
		"<office:text>\n");

	odt_write_blocks(&content, doc->blocks);

	buf_puts(&content, "</office:text>\n</office:body>\n</office:document-content>");

	char *content_xml = buf_finish(&content);
	zip_add(zip, "content.xml", content_xml, strlen(content_xml));

	return zip_write(zip, out_len);
}

// ============================================================================
// EPUB Writer
// ============================================================================

static void epub_write_inlines(struct buffer *buf, struct inline_node *node)
{
	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				html_escape(buf, node->text);
				break;
			case INLINE_BOLD:
				buf_puts(buf, "<strong>");
				epub_write_inlines(buf, node->children);
				buf_puts(buf, "</strong>");
				break;
			case INLINE_ITALIC:
				buf_puts(buf, "<em>");
				epub_write_inlines(buf, node->children);
				buf_puts(buf, "</em>");
				break;
			case INLINE_CODE:
				buf_puts(buf, "<code>");
				html_escape(buf, node->text);
				buf_puts(buf, "</code>");
				break;
			case INLINE_LINK:
				buf_puts(buf, "<a href=\"");
				html_escape(buf, node->text);
				buf_puts(buf, "\">");
				epub_write_inlines(buf, node->children);
				buf_puts(buf, "</a>");
				break;
			case INLINE_IMAGE:
				buf_puts(buf, "<img src=\"");
				html_escape(buf, node->text);
				buf_puts(buf, "\" alt=\"");
				if (node->title) html_escape(buf, node->title);
				buf_puts(buf, "\"/>");
				break;
			case INLINE_LINEBREAK:
				buf_puts(buf, "<br/>\n");
				break;
		}
		node = node->next;
	}
}

static void epub_write_blocks(struct buffer *buf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				buf_puts(buf, "<p>");
				epub_write_inlines(buf, node->inlines);
				buf_puts(buf, "</p>\n");
				break;
			case BLOCK_HEADING:
				buf_printf(buf, "<h%d>", node->heading.level);
				epub_write_inlines(buf, node->inlines);
				buf_printf(buf, "</h%d>\n", node->heading.level);
				break;
			case BLOCK_CODE_BLOCK:
				if (node->code_block.language) {
					buf_printf(buf, "<pre><code class=\"language-%s\">", node->code_block.language);
				} else {
					buf_puts(buf, "<pre><code>");
				}
				html_escape(buf, node->code_block.code);
				buf_puts(buf, "</code></pre>\n");
				break;
			case BLOCK_BLOCKQUOTE:
				buf_puts(buf, "<blockquote>\n");
				epub_write_blocks(buf, node->children);
				buf_puts(buf, "</blockquote>\n");
				break;
			case BLOCK_LIST:
				buf_puts(buf, node->list.ordered ? "<ol>\n" : "<ul>\n");
				{
					struct block_node *item = node->children;
					while (item) {
						buf_puts(buf, "<li>");
						if (item->children && item->children->type == BLOCK_PARAGRAPH && !item->children->next) {
							epub_write_inlines(buf, item->children->inlines);
						} else {
							epub_write_blocks(buf, item->children);
						}
						buf_puts(buf, "</li>\n");
						item = item->next;
					}
				}
				buf_puts(buf, node->list.ordered ? "</ol>\n" : "</ul>\n");
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_puts(buf, "<hr/>\n");
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_epub(struct document *doc, size_t *out_len)
{
	struct zip_archive *zip = arena_zalloc(sizeof(*zip));
	zip->entries = arena_alloc(8 * sizeof(*zip->entries));
	zip->cap_entries = 8;

	// mimetype (must be first)
	const char *mimetype = "application/epub+zip";
	zip_add(zip, "mimetype", mimetype, strlen(mimetype));

	// META-INF/container.xml
	const char *container =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
		"  <rootfiles>\n"
		"    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
		"  </rootfiles>\n"
		"</container>";
	zip_add(zip, "META-INF/container.xml", container, strlen(container));

	// OEBPS/content.opf
	struct buffer opf;
	buf_init(&opf);
	buf_puts(&opf,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"BookId\" version=\"2.0\">\n"
		"  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
		"    <dc:identifier id=\"BookId\">urn:uuid:doc-converter-output</dc:identifier>\n");
	if (doc->title) {
		buf_puts(&opf, "    <dc:title>");
		html_escape(&opf, doc->title);
		buf_puts(&opf, "</dc:title>\n");
	} else {
		buf_puts(&opf, "    <dc:title>Untitled</dc:title>\n");
	}
	buf_puts(&opf,
		"    <dc:language>en</dc:language>\n"
		"  </metadata>\n"
		"  <manifest>\n"
		"    <item id=\"chapter1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>\n"
		"    <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n"
		"  </manifest>\n"
		"  <spine toc=\"ncx\">\n"
		"    <itemref idref=\"chapter1\"/>\n"
		"  </spine>\n"
		"</package>");
	char *opf_xml = buf_finish(&opf);
	zip_add(zip, "OEBPS/content.opf", opf_xml, strlen(opf_xml));

	// OEBPS/toc.ncx (navigation)
	struct buffer ncx;
	buf_init(&ncx);
	buf_puts(&ncx,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
		"  <head>\n"
		"    <meta name=\"dtb:uid\" content=\"urn:uuid:doc-converter-output\"/>\n"
		"  </head>\n"
		"  <docTitle><text>");
	if (doc->title) html_escape(&ncx, doc->title);
	else buf_puts(&ncx, "Untitled");
	buf_puts(&ncx,
		"</text></docTitle>\n"
		"  <navMap>\n"
		"    <navPoint id=\"chapter1\" playOrder=\"1\">\n"
		"      <navLabel><text>Content</text></navLabel>\n"
		"      <content src=\"chapter1.xhtml\"/>\n"
		"    </navPoint>\n"
		"  </navMap>\n"
		"</ncx>");
	char *ncx_xml = buf_finish(&ncx);
	zip_add(zip, "OEBPS/toc.ncx", ncx_xml, strlen(ncx_xml));

	// OEBPS/chapter1.xhtml (main content)
	struct buffer chapter;
	buf_init(&chapter);
	buf_puts(&chapter,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
		"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
		"<head>\n"
		"  <meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=utf-8\"/>\n"
		"  <title>");
	if (doc->title) html_escape(&chapter, doc->title);
	else buf_puts(&chapter, "Untitled");
	buf_puts(&chapter, "</title>\n</head>\n<body>\n");

	epub_write_blocks(&chapter, doc->blocks);

	buf_puts(&chapter, "</body>\n</html>");
	char *chapter_xhtml = buf_finish(&chapter);
	zip_add(zip, "OEBPS/chapter1.xhtml", chapter_xhtml, strlen(chapter_xhtml));

	return zip_write(zip, out_len);
}

#endif /* HAVE_ZLIB */

// ============================================================================
// PDF Writer (simple implementation)
// ============================================================================

#define PDF_PAGE_WIDTH 612
#define PDF_PAGE_HEIGHT 792
#define PDF_MARGIN_LEFT 50
#define PDF_MARGIN_RIGHT 50
#define PDF_MARGIN_TOP 750
#define PDF_MARGIN_BOTTOM 50

struct pdf_writer {
	struct buffer buf;
	size_t *offsets;
	int num_objects;
	int cap_objects;
	int page_content_obj;
	struct buffer content;
	int y_pos;
	int font_size;
};

static void pdf_init(struct pdf_writer *pdf)
{
	buf_init(&pdf->buf);
	buf_init(&pdf->content);
	pdf->offsets = NULL;
	pdf->num_objects = 0;
	pdf->cap_objects = 0;
	pdf->y_pos = PDF_MARGIN_TOP;
	pdf->font_size = 12;

	buf_puts(&pdf->buf, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");
}

static int pdf_add_object(struct pdf_writer *pdf)
{
	if (pdf->num_objects >= pdf->cap_objects) {
		pdf->cap_objects = pdf->cap_objects ? pdf->cap_objects * 2 : 16;
		void *new_offsets = realloc(pdf->offsets, (size_t)pdf->cap_objects * sizeof(size_t));
		if (!new_offsets) {
			PUTS_ERR("Error: out of memory\n");
			exit(1);
		}
		pdf->offsets = new_offsets;
	}
	pdf->offsets[pdf->num_objects] = pdf->buf.len;
	return ++pdf->num_objects;
}

static void pdf_escape_string(struct buffer *buf, const char *s)
{
	buf_putc(buf, '(');
	while (*s) {
		if (*s == '(' || *s == ')' || *s == '\\')
			buf_putc(buf, '\\');
		buf_putc(buf, *s++);
	}
	buf_putc(buf, ')');
}

static void pdf_write_text(struct pdf_writer *pdf, const char *text, bool bold, bool italic)
{
	if (pdf->y_pos < PDF_MARGIN_BOTTOM) {
		// New page needed - simplified, just continue
		pdf->y_pos = PDF_MARGIN_TOP;
	}

	buf_puts(&pdf->content, "BT\n");
	buf_printf(&pdf->content, "/F%d %d Tf\n", (bold ? 2 : 1) + (italic ? 2 : 0), pdf->font_size);
	buf_printf(&pdf->content, "%d %d Td\n", PDF_MARGIN_LEFT, pdf->y_pos);
	pdf_escape_string(&pdf->content, text);
	buf_puts(&pdf->content, " Tj\nET\n");

	pdf->y_pos -= pdf->font_size + 4;
}

static void pdf_write_inlines(struct pdf_writer *pdf, struct inline_node *node, bool bold, bool italic)
{
	struct buffer line;
	buf_init(&line);

	while (node) {
		switch (node->type) {
			case INLINE_TEXT:
				buf_puts(&line, node->text);
				break;
			case INLINE_BOLD:
				if (line.len > 0) {
					buf_putc(&line, '\0');
					pdf_write_text(pdf, line.data, bold, italic);
					line.len = 0;
				}
				pdf_write_inlines(pdf, node->children, true, italic);
				break;
			case INLINE_ITALIC:
				if (line.len > 0) {
					buf_putc(&line, '\0');
					pdf_write_text(pdf, line.data, bold, italic);
					line.len = 0;
				}
				pdf_write_inlines(pdf, node->children, bold, true);
				break;
			case INLINE_CODE:
				buf_puts(&line, node->text);
				break;
			case INLINE_LINK:
				pdf_write_inlines(pdf, node->children, bold, italic);
				break;
			case INLINE_IMAGE:
				// Skip images in simple PDF
				break;
			case INLINE_LINEBREAK:
				if (line.len > 0) {
					buf_putc(&line, '\0');
					pdf_write_text(pdf, line.data, bold, italic);
					line.len = 0;
				}
				break;
		}
		node = node->next;
	}

	if (line.len > 0) {
		buf_putc(&line, '\0');
		pdf_write_text(pdf, line.data, bold, italic);
	}
	buf_free(&line);
}

static void pdf_write_blocks(struct pdf_writer *pdf, struct block_node *node)
{
	while (node) {
		switch (node->type) {
			case BLOCK_PARAGRAPH:
				pdf->font_size = 12;
				pdf_write_inlines(pdf, node->inlines, false, false);
				pdf->y_pos -= 8;  // Paragraph spacing
				break;
			case BLOCK_HEADING:
				pdf->font_size = 24 - (node->heading.level - 1) * 2;
				pdf_write_inlines(pdf, node->inlines, true, false);
				pdf->font_size = 12;
				pdf->y_pos -= 12;
				break;
			case BLOCK_CODE_BLOCK:
				// Write code as-is with monospace feel
				{
					char *code = node->code_block.code;
					char *line = code;
					while (*line) {
						char *end = strchr(line, '\n');
						if (end) {
							char save = *end;
							*end = '\0';
							pdf_write_text(pdf, line, false, false);
							*end = save;
							line = end + 1;
						} else {
							pdf_write_text(pdf, line, false, false);
							break;
						}
					}
				}
				pdf->y_pos -= 8;
				break;
			case BLOCK_BLOCKQUOTE:
				// Simple indent
				pdf_write_blocks(pdf, node->children);
				pdf->y_pos -= 8;
				break;
			case BLOCK_LIST:
				{
					struct block_node *item = node->children;
					int num = node->list.start;
					while (item) {
						struct buffer prefix;
						buf_init(&prefix);
						if (node->list.ordered) {
							buf_printf(&prefix, "%d. ", num++);
						} else {
							buf_puts(&prefix, "* ");
						}
						buf_putc(&prefix, '\0');
						pdf_write_text(pdf, prefix.data, false, false);
						buf_free(&prefix);

						if (item->children && item->children->type == BLOCK_PARAGRAPH) {
							pdf_write_inlines(pdf, item->children->inlines, false, false);
						}
						item = item->next;
					}
				}
				pdf->y_pos -= 8;
				break;
			case BLOCK_LIST_ITEM:
				break;
			case BLOCK_THEMATIC_BREAK:
				buf_printf(&pdf->content, "q\n0.5 w\n%d %d m %d %d l S\nQ\n",
					PDF_MARGIN_LEFT, pdf->y_pos,
					PDF_PAGE_WIDTH - PDF_MARGIN_RIGHT, pdf->y_pos);
				pdf->y_pos -= 20;
				break;
			case BLOCK_TABLE:
			case BLOCK_TABLE_ROW:
			case BLOCK_TABLE_CELL:
				break;
		}
		node = node->next;
	}
}

static char *write_pdf(struct document *doc, size_t *out_len)
{
	struct pdf_writer pdf;
	pdf_init(&pdf);

	// Object 1: Catalog
	int catalog = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", catalog);

	// Object 2: Pages
	int pages = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n", pages);

	// Object 3: Page
	int page = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Contents 5 0 R /Resources << /Font << /F1 4 0 R /F2 6 0 R >> >> >>\nendobj\n", page, PDF_PAGE_WIDTH, PDF_PAGE_HEIGHT);

	// Object 4: Font (regular)
	int font1 = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n", font1);

	// Generate content
	pdf_write_blocks(&pdf, doc->blocks);

	// Object 5: Content stream
	int content = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Length %zu >>\nstream\n", content, pdf.content.len);
	buf_write(&pdf.buf, pdf.content.data, pdf.content.len);
	buf_puts(&pdf.buf, "endstream\nendobj\n");

	// Object 6: Font (bold)
	int font2 = pdf_add_object(&pdf);
	buf_printf(&pdf.buf, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>\nendobj\n", font2);

	// Cross-reference table
	size_t xref_offset = pdf.buf.len;
	buf_puts(&pdf.buf, "xref\n");
	buf_printf(&pdf.buf, "0 %d\n", pdf.num_objects + 1);
	buf_puts(&pdf.buf, "0000000000 65535 f \n");
	for (int i = 0; i < pdf.num_objects; i++) {
		if (pdf.offsets[i] > 9999999999ULL) {
			PUTS_ERR("Error: PDF too large for 10-digit xref offsets\n");
			exit(1);
		}
		buf_printf(&pdf.buf, "%010zu 00000 n \n", pdf.offsets[i]);
	}

	// Trailer
	buf_puts(&pdf.buf, "trailer\n");
	buf_printf(&pdf.buf, "<< /Size %d /Root 1 0 R >>\n", pdf.num_objects + 1);
	buf_puts(&pdf.buf, "startxref\n");
	buf_printf(&pdf.buf, "%zu\n", xref_offset);
	buf_puts(&pdf.buf, "%%EOF\n");

	buf_free(&pdf.content);
	free(pdf.offsets);

	*out_len = pdf.buf.len;
	return pdf.buf.data;
}

// ============================================================================
// EML Parser
// ============================================================================

// Decode base64 character to 6-bit value, or -1 if invalid
static int base64_decode_char(char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

// Decode base64 data into buffer
static void base64_decode(struct buffer *buf, const char *data, size_t len)
{
	unsigned int accum = 0;
	int bits = 0;

	for (size_t i = 0; i < len; i++) {
		if (data[i] == '\r' || data[i] == '\n' || data[i] == ' ')
			continue;
		if (data[i] == '=')
			break;

		int val = base64_decode_char(data[i]);
		if (val < 0)
			continue;

	accum = (accum << 6) | (unsigned int)val;
		bits += 6;

		if (bits >= 8) {
			bits -= 8;
			buf_putc(buf, (char)((accum >> bits) & 0xFFu));
		}
	}
}

// Decode quoted-printable data into buffer
static void quoted_printable_decode(struct buffer *buf, const char *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (data[i] == '=' && i + 2 < len) {
			if (data[i + 1] == '\r' || data[i + 1] == '\n') {
				// Soft line break - skip
				i++;
				if (data[i] == '\r' && i + 1 < len && data[i + 1] == '\n')
					i++;
			} else if (isxdigit(data[i + 1]) && isxdigit(data[i + 2])) {
				// Hex-encoded byte
				char hex[3] = { data[i + 1], data[i + 2], '\0' };
				buf_putc(buf, (char)strtol(hex, NULL, 16));
				i += 2;
			} else {
				buf_putc(buf, data[i]);
			}
		} else {
			buf_putc(buf, data[i]);
		}
	}
}

// Skip a header line (handles folded headers with leading whitespace)
static const char *eml_skip_header_line(const char *p, const char *end)
{
	while (p < end) {
		if (*p == '\n') {
			p++;
			// Check for folded header (line starting with space/tab)
			if (p < end && (*p == ' ' || *p == '\t'))
				continue;
			return p;
		}
		if (*p == '\r' && p + 1 < end && p[1] == '\n') {
			p += 2;
			if (p < end && (*p == ' ' || *p == '\t'))
				continue;
			return p;
		}
		p++;
	}
	return end;
}

// Get header value (handles folded headers by collapsing to single line)
static char *eml_get_header_value(const char *line, const char *end, const char *name)
{
	size_t name_len = strlen(name);
	if (strncasecmp(line, name, name_len) != 0 || line[name_len] != ':')
		return NULL;

	const char *val_start = line + name_len + 1;
	while (val_start < end && (*val_start == ' ' || *val_start == '\t'))
		val_start++;

	const char *val_end = eml_skip_header_line(line, end);

	// Build value, collapsing folded lines - use temp buffer then copy to arena
	struct buffer val;
	buf_init(&val);

	const char *p = val_start;
	while (p < val_end) {
		if (*p == '\r' || *p == '\n') {
			// Skip CRLF/LF and following whitespace
			while (p < val_end && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t'))
				p++;
			if (p < val_end)
				buf_putc(&val, ' ');
		} else {
			buf_putc(&val, *p);
			p++;
		}
	}

	// Trim trailing whitespace
	while (val.len > 0 && (val.data[val.len - 1] == ' ' || val.data[val.len - 1] == '\t' ||
						  val.data[val.len - 1] == '\r' || val.data[val.len - 1] == '\n'))
		val.len--;

	char *result = arena_strndup(val.data, val.len);
	buf_free(&val);
	return result;
}

// Find boundary in multipart content-type header
static char *eml_find_boundary(const char *content_type)
{
	const char *p = strcasestr(content_type, "boundary=");
	if (!p) return NULL;
	p += 9;

	// Skip optional quote
	char quote = 0;
	if (*p == '"' || *p == '\'') {
		quote = *p;
		p++;
	}

	const char *start = p;
	while (*p && (quote ? *p != quote : (*p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')))
		p++;

	return arena_strndup(start, (size_t)(p - start));
}

// Parse a single MIME part and add to document
static void eml_parse_part(struct document *doc, const char *data, size_t len,
						  struct block_node **last_block)
{
	const char *end = data + len;
	const char *p = data;

	// Parse part headers
	char *content_type = NULL;
	char *encoding = NULL;

	while (p < end) {
		// Check for end of headers
		if (*p == '\n' || (*p == '\r' && p + 1 < end && p[1] == '\n')) {
			if (*p == '\r') p++;
			p++;
			break;
		}

		const char *line_end = eml_skip_header_line(p, end);

		char *val;
		if ((val = eml_get_header_value(p, end, "Content-Type"))) {
			content_type = val;
		} else if ((val = eml_get_header_value(p, end, "Content-Transfer-Encoding"))) {
			encoding = val;
		}

		p = line_end;
	}

	// Body starts at p
	const char *body = p;
	size_t body_len = (size_t)(end - body);

	// Decode body if needed
	struct buffer decoded;
	buf_init(&decoded);

	if (encoding && strcasecmp(encoding, "base64") == 0) {
		base64_decode(&decoded, body, body_len);
		buf_putc(&decoded, '\0');
		body = decoded.data;
		body_len = decoded.len - 1;
	} else if (encoding && strcasecmp(encoding, "quoted-printable") == 0) {
		quoted_printable_decode(&decoded, body, body_len);
		buf_putc(&decoded, '\0');
		body = decoded.data;
		body_len = decoded.len - 1;
	}

	// Check if multipart
	if (content_type && strncasecmp(content_type, "multipart/", 10) == 0) {
		char *boundary = eml_find_boundary(content_type);
		if (boundary) {
			size_t boundary_len = strlen(boundary);
			char *delim = arena_alloc(boundary_len + 3);
			delim[0] = '-';
			delim[1] = '-';
			memcpy(delim + 2, boundary, boundary_len + 1);
			size_t delim_len = boundary_len + 2;

			const char *part_start = NULL;
			p = body;

			while (p < body + body_len) {
				// Find next boundary
				if (strncmp(p, delim, delim_len) == 0) {
					if (part_start && p > part_start) {
						// Process previous part
							size_t part_len = (size_t)(p - part_start);
						// Trim trailing CRLF before boundary
						while (part_len > 0 && (part_start[part_len - 1] == '\r' || part_start[part_len - 1] == '\n'))
							part_len--;
						eml_parse_part(doc, part_start, part_len, last_block);
					}

					p += delim_len;
					// Check for terminator
					if (p + 1 < body + body_len && p[0] == '-' && p[1] == '-')
						break;

					// Skip to start of part
					while (p < body + body_len && *p != '\n')
						p++;
					if (p < body + body_len)
						p++;
					part_start = p;
				} else {
					p++;
				}
			}
		}
	} else if (!content_type || strncasecmp(content_type, "text/", 5) == 0) {
		// Text part - add as paragraph(s)
		// Split into paragraphs on blank lines
		const char *para_start = body;
		const char *body_end = body + body_len;
		p = body;

		while (p <= body_end) {
			bool at_end = (p >= body_end);
			bool blank_line = (!at_end && *p == '\n') ||
							 (!at_end && p + 1 < body_end && *p == '\r' && p[1] == '\n');

			if (at_end || blank_line) {
				// End of paragraph
					size_t para_len = (size_t)(p - para_start);
				// Trim trailing whitespace
				while (para_len > 0 && (para_start[para_len - 1] == '\r' ||
					   para_start[para_len - 1] == '\n' || para_start[para_len - 1] == ' '))
					para_len--;

				if (para_len > 0) {
					struct block_node *para = block_new(BLOCK_PARAGRAPH);
					para->inlines = inline_new(INLINE_TEXT);
					para->inlines->text = arena_strndup(para_start, para_len);

					if (*last_block)
						(*last_block)->next = para;
					else
						doc->blocks = para;
					*last_block = para;
				}

				if (at_end)
					break;

				// Skip blank lines
				while (p < body_end && (*p == '\r' || *p == '\n'))
					p++;
				para_start = p;
			} else {
				p++;
			}
		}
	}
	// Skip non-text parts (attachments, etc.)

	buf_free(&decoded);
}

static struct document *parse_eml(const char *input, size_t len)
{
	struct document *doc = arena_zalloc(sizeof(*doc));
	struct block_node *last_block = NULL;

	const char *end = input + len;
	const char *p = input;

	// Parse email headers
	char *subject = NULL;
	char *from = NULL;
	char *date = NULL;
	char *content_type = NULL;
	char *encoding = NULL;

	while (p < end) {
		// Check for end of headers (blank line)
		if (*p == '\n' || (*p == '\r' && p + 1 < end && p[1] == '\n')) {
			if (*p == '\r') p++;
			p++;
			break;
		}

		const char *line_end = eml_skip_header_line(p, end);

		char *val;
		if ((val = eml_get_header_value(p, end, "Subject"))) {
			subject = val;
		} else if ((val = eml_get_header_value(p, end, "From"))) {
			from = val;
		} else if ((val = eml_get_header_value(p, end, "Date"))) {
			date = val;
		} else if ((val = eml_get_header_value(p, end, "Content-Type"))) {
			content_type = val;
		} else if ((val = eml_get_header_value(p, end, "Content-Transfer-Encoding"))) {
			encoding = val;
		}

		p = line_end;
	}

	// Add subject as heading
	if (subject) {
		struct block_node *heading = block_new(BLOCK_HEADING);
		heading->heading.level = 1;
		heading->inlines = inline_new(INLINE_TEXT);
		heading->inlines->text = subject;
		doc->blocks = heading;
		last_block = heading;
		doc->title = arena_strdup(subject);
	}

	// Add metadata as paragraph
	if (from || date) {
		struct buffer meta;
		buf_init(&meta);
		if (from) {
			buf_puts(&meta, "From: ");
			buf_puts(&meta, from);
		}
		if (from && date)
			buf_puts(&meta, "  \n");
		if (date) {
			buf_puts(&meta, "Date: ");
			buf_puts(&meta, date);
		}
		buf_putc(&meta, '\0');

		struct block_node *para = block_new(BLOCK_PARAGRAPH);
		para->inlines = inline_new(INLINE_ITALIC);
		para->inlines->children = inline_new(INLINE_TEXT);
		para->inlines->children->text = meta.data;

		if (last_block)
			last_block->next = para;
		else
			doc->blocks = para;
		last_block = para;
	}

	// Add horizontal rule after header
	if (last_block) {
		struct block_node *hr = block_new(BLOCK_THEMATIC_BREAK);
		last_block->next = hr;
		last_block = hr;
	}

	// Parse body
	const char *body = p;
	size_t body_len = (size_t)(end - body);

	// Decode body if needed
	struct buffer decoded;
	buf_init(&decoded);

	if (encoding && strcasecmp(encoding, "base64") == 0) {
		base64_decode(&decoded, body, body_len);
		buf_putc(&decoded, '\0');
		body = decoded.data;
		body_len = decoded.len - 1;
	} else if (encoding && strcasecmp(encoding, "quoted-printable") == 0) {
		quoted_printable_decode(&decoded, body, body_len);
		buf_putc(&decoded, '\0');
		body = decoded.data;
		body_len = decoded.len - 1;
	}

	// Check if multipart
	if (content_type && strncasecmp(content_type, "multipart/", 10) == 0) {
		// Pass body with fake headers so eml_parse_part can see the Content-Type
		struct buffer fake;
		buf_init(&fake);
		buf_puts(&fake, "Content-Type: ");
		buf_puts(&fake, content_type);
		buf_puts(&fake, "\r\n\r\n");
		buf_write(&fake, body, body_len);
		eml_parse_part(doc, fake.data, fake.len, &last_block);
		buf_free(&fake);
	} else {
		// Simple text body - add paragraphs directly
		const char *body_end = body + body_len;
		const char *para_start = body;
		p = body;

		while (p <= body_end) {
			bool at_end = (p >= body_end);
			bool blank_line = (!at_end && *p == '\n') ||
							 (!at_end && p + 1 < body_end && *p == '\r' && p[1] == '\n');

			if (at_end || blank_line) {
					size_t para_len = (size_t)(p - para_start);
				while (para_len > 0 && (para_start[para_len - 1] == '\r' ||
					   para_start[para_len - 1] == '\n' || para_start[para_len - 1] == ' '))
					para_len--;

				if (para_len > 0) {
					struct block_node *para = block_new(BLOCK_PARAGRAPH);
					para->inlines = inline_new(INLINE_TEXT);
					para->inlines->text = arena_strndup(para_start, para_len);

					if (last_block)
						last_block->next = para;
					else
						doc->blocks = para;
					last_block = para;
				}

				if (at_end)
					break;

				while (p < body_end && (*p == '\r' || *p == '\n'))
					p++;
				para_start = p;
			} else {
				p++;
			}
		}
	}

	buf_free(&decoded);

	return doc;
}

// ============================================================================
// File I/O
// ============================================================================

static char *read_file(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	long end = ftell(f);
	if (end < 0 || (unsigned long)end > SIZE_MAX - 1) {
		fclose(f);
		return NULL;
	}
	if (max_bytes != 0 && (size_t)end > max_bytes) {
		errno = EFBIG;
		fclose(f);
		return NULL;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	*len = (size_t)end;

	char *data = malloc(*len + 1);
	if (!data) {
		fclose(f);
		return NULL;
	}
	if (*len > 0 && READ_FILE(data, *len, f) != *len) {
		free(data);
		fclose(f);
		return NULL;
	}
	data[*len] = '\0';
	if (fclose(f) != 0) {
		free(data);
		return NULL;
	}

	return data;
}

static bool write_file(const char *path, const char *data, size_t len)
{
	FILE *f = fopen(path, "wb");
	if (!f) return false;

	if (len > 0 && WRITE_FILE(data, len, f) != len) {
		fclose(f);
		return false;
	}
	if (fclose(f) != 0)
		return false;
	return true;
}

// ============================================================================
// Format detection
// ============================================================================

enum format {
	FMT_UNKNOWN,
	FMT_MARKDOWN,
	FMT_HTML,
	FMT_DOCX,
	FMT_PDF,
	FMT_EML,
	FMT_TEXT,
	FMT_RTF,
	FMT_LATEX,
	FMT_ODT,
	FMT_EPUB,
	FMT_JSON,
	FMT_RST,
	FMT_ASCIIDOC,
	FMT_ORG,
	FMT_TEXTILE,
	FMT_MEDIAWIKI,
	FMT_CREOLE,
	FMT_DOKUWIKI,
	FMT_JIRA,
	FMT_BBCODE,
	FMT_GEMTEXT,
	FMT_DJOT,
	FMT_MAN,
	FMT_TEXINFO,
	FMT_POD,
	FMT_DOCBOOK,
	FMT_TYPST,
	FMT_OPML,
	FMT_MUSE
};

static bool write_stream_output(const char *path, struct document *doc, enum format fmt)
{
	FILE *f = fopen(path, "wb");
	if (!f) return false;

	output_stream = f;
	switch (fmt) {
		case FMT_MARKDOWN:  (void)write_markdown(doc); break;
		case FMT_HTML:      (void)write_html(doc); break;
		case FMT_TEXT:      (void)write_text(doc); break;
		case FMT_RTF:       (void)write_rtf(doc); break;
		case FMT_LATEX:     (void)write_latex(doc); break;
		case FMT_JSON:      (void)write_json(doc); break;
		case FMT_RST:       (void)write_rst(doc); break;
		case FMT_ASCIIDOC:  (void)write_asciidoc(doc); break;
		case FMT_ORG:       (void)write_org(doc); break;
		case FMT_TEXTILE:   (void)write_textile(doc); break;
		case FMT_MEDIAWIKI: (void)write_mediawiki(doc); break;
		case FMT_CREOLE:    (void)write_creole(doc); break;
		case FMT_DOKUWIKI:  (void)write_dokuwiki(doc); break;
		case FMT_JIRA:      (void)write_jira(doc); break;
		case FMT_BBCODE:    (void)write_bbcode(doc); break;
		case FMT_GEMTEXT:   (void)write_gemtext(doc); break;
		case FMT_DJOT:      (void)write_djot(doc); break;
		case FMT_MAN:       (void)write_man(doc); break;
		case FMT_TEXINFO:   (void)write_texinfo(doc); break;
		case FMT_POD:       (void)write_pod(doc); break;
		case FMT_DOCBOOK:   (void)write_docbook(doc); break;
		case FMT_TYPST:     (void)write_typst(doc); break;
		case FMT_OPML:      (void)write_opml(doc); break;
		case FMT_MUSE:      (void)write_muse(doc); break;
		default:
			output_stream = NULL;
			fclose(f);
			return false;
	}
	output_stream = NULL;

	if (ferror(f)) {
		fclose(f);
		return false;
	}
	return fclose(f) == 0;
}

static enum format detect_format(const char *path, const char *data, size_t len)
{
	// Check extension first
	const char *ext = strrchr(path, '.');
	if (ext) {
		if (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".markdown") == 0)
			return FMT_MARKDOWN;
		if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
			return FMT_HTML;
		if (strcasecmp(ext, ".docx") == 0)
			return FMT_DOCX;
		if (strcasecmp(ext, ".pdf") == 0)
			return FMT_PDF;
		if (strcasecmp(ext, ".eml") == 0)
			return FMT_EML;
		if (strcasecmp(ext, ".txt") == 0)
			return FMT_TEXT;
		if (strcasecmp(ext, ".rtf") == 0)
			return FMT_RTF;
		if (strcasecmp(ext, ".tex") == 0 || strcasecmp(ext, ".latex") == 0)
			return FMT_LATEX;
		if (strcasecmp(ext, ".odt") == 0)
			return FMT_ODT;
		if (strcasecmp(ext, ".epub") == 0)
			return FMT_EPUB;
		if (strcasecmp(ext, ".json") == 0)
			return FMT_JSON;
		if (strcasecmp(ext, ".rst") == 0)
			return FMT_RST;
		if (strcasecmp(ext, ".adoc") == 0 || strcasecmp(ext, ".asciidoc") == 0)
			return FMT_ASCIIDOC;
		if (strcasecmp(ext, ".org") == 0)
			return FMT_ORG;
		if (strcasecmp(ext, ".textile") == 0)
			return FMT_TEXTILE;
		if (strcasecmp(ext, ".mediawiki") == 0)
			return FMT_MEDIAWIKI;
		if (strcasecmp(ext, ".creole") == 0)
			return FMT_CREOLE;
		if (strcasecmp(ext, ".dokuwiki") == 0)
			return FMT_DOKUWIKI;
		if (strcasecmp(ext, ".jira") == 0)
			return FMT_JIRA;
		if (strcasecmp(ext, ".bbcode") == 0)
			return FMT_BBCODE;
		if (strcasecmp(ext, ".gmi") == 0 || strcasecmp(ext, ".gemini") == 0)
			return FMT_GEMTEXT;
		if (strcasecmp(ext, ".djot") == 0)
			return FMT_DJOT;
		// Man pages: .1 through .9
		if (ext[1] >= '1' && ext[1] <= '9' && ext[2] == '\0')
			return FMT_MAN;
		if (strcasecmp(ext, ".texi") == 0 || strcasecmp(ext, ".texinfo") == 0)
			return FMT_TEXINFO;
		if (strcasecmp(ext, ".pod") == 0)
			return FMT_POD;
		if (strcasecmp(ext, ".xml") == 0)
			return FMT_DOCBOOK;
		if (strcasecmp(ext, ".typ") == 0)
			return FMT_TYPST;
		if (strcasecmp(ext, ".opml") == 0)
			return FMT_OPML;
		if (strcasecmp(ext, ".muse") == 0)
			return FMT_MUSE;
	}

	// Check content
	if (len >= 4 && memcmp(data, "PK\x03\x04", 4) == 0)
		return FMT_DOCX;  // ZIP signature (DOCX/ODT/EPUB)
	if (len >= 5 && memcmp(data, "%PDF-", 5) == 0)
		return FMT_PDF;
	if (len >= 5 && memcmp(data, "{\\rtf", 5) == 0)
		return FMT_RTF;
	if (strstr(data, "<!DOCTYPE html") || strstr(data, "<html") || strstr(data, "<HTML"))
		return FMT_HTML;
	// Check for email headers
	if (data && (strncmp(data, "From:", 5) == 0 || strncmp(data, "Date:", 5) == 0 ||
				 strncmp(data, "Subject:", 8) == 0 || strncmp(data, "MIME-Version:", 13) == 0))
		return FMT_EML;
	// Check for LaTeX
	if (data && strstr(data, "\\documentclass"))
		return FMT_LATEX;
	// Check for JSON
	if (data && (data[0] == '{' || data[0] == '['))
		return FMT_JSON;

	// Default to Markdown
	return FMT_MARKDOWN;
}

static enum format format_from_name(const char *name)
{
	if (strcasecmp(name, "md") == 0 || strcasecmp(name, "markdown") == 0)
		return FMT_MARKDOWN;
	if (strcasecmp(name, "html") == 0 || strcasecmp(name, "htm") == 0)
		return FMT_HTML;
	if (strcasecmp(name, "docx") == 0)
		return FMT_DOCX;
	if (strcasecmp(name, "pdf") == 0)
		return FMT_PDF;
	if (strcasecmp(name, "eml") == 0 || strcasecmp(name, "email") == 0)
		return FMT_EML;
	if (strcasecmp(name, "txt") == 0 || strcasecmp(name, "text") == 0)
		return FMT_TEXT;
	if (strcasecmp(name, "rtf") == 0)
		return FMT_RTF;
	if (strcasecmp(name, "tex") == 0 || strcasecmp(name, "latex") == 0)
		return FMT_LATEX;
	if (strcasecmp(name, "odt") == 0)
		return FMT_ODT;
	if (strcasecmp(name, "epub") == 0)
		return FMT_EPUB;
	if (strcasecmp(name, "json") == 0)
		return FMT_JSON;
	if (strcasecmp(name, "rst") == 0)
		return FMT_RST;
	if (strcasecmp(name, "adoc") == 0 || strcasecmp(name, "asciidoc") == 0)
		return FMT_ASCIIDOC;
	if (strcasecmp(name, "org") == 0)
		return FMT_ORG;
	if (strcasecmp(name, "textile") == 0)
		return FMT_TEXTILE;
	if (strcasecmp(name, "mediawiki") == 0 || strcasecmp(name, "wiki") == 0)
		return FMT_MEDIAWIKI;
	if (strcasecmp(name, "creole") == 0)
		return FMT_CREOLE;
	if (strcasecmp(name, "dokuwiki") == 0)
		return FMT_DOKUWIKI;
	if (strcasecmp(name, "jira") == 0 || strcasecmp(name, "confluence") == 0)
		return FMT_JIRA;
	if (strcasecmp(name, "bbcode") == 0)
		return FMT_BBCODE;
	if (strcasecmp(name, "gemtext") == 0 || strcasecmp(name, "gmi") == 0)
		return FMT_GEMTEXT;
	if (strcasecmp(name, "djot") == 0)
		return FMT_DJOT;
	if (strcasecmp(name, "man") == 0 || strcasecmp(name, "troff") == 0 || strcasecmp(name, "groff") == 0)
		return FMT_MAN;
	if (strcasecmp(name, "texinfo") == 0 || strcasecmp(name, "texi") == 0)
		return FMT_TEXINFO;
	if (strcasecmp(name, "pod") == 0)
		return FMT_POD;
	if (strcasecmp(name, "docbook") == 0 || strcasecmp(name, "xml") == 0)
		return FMT_DOCBOOK;
	if (strcasecmp(name, "typst") == 0 || strcasecmp(name, "typ") == 0)
		return FMT_TYPST;
	if (strcasecmp(name, "opml") == 0)
		return FMT_OPML;
	if (strcasecmp(name, "muse") == 0)
		return FMT_MUSE;
	return FMT_UNKNOWN;
}

// ============================================================================
// Main
// ============================================================================

static void print_help(void)
{
	PUTS(
		"Usage: doc-converter [OPTIONS] INPUT [OUTPUT]\n"
		"\n"
		"Convert documents between various formats.\n"
		"\n"
		"Options:\n"
		"  -f, --from FORMAT    Input format\n"
		"  -t, --to FORMAT      Output format\n"
		"  -o, --output FILE    Output file (required for binary formats)\n"
		"  -B, --max-bytes N    Fail if input file size > N (0 = unlimited)\n"
		"  -S, --strict         Fail on unsupported constructs\n"
		"  -h, --help           Show this help\n"
		"\n"
		"Supported Formats:\n"
		"  Format      Name(s)            Read  Write  Extension(s)\n"
		"  ----------  -----------------  ----  -----  ----------------\n"
		"  Markdown    md, markdown        *      *    .md, .markdown\n"
		"  HTML        html, htm           *      *    .html, .htm\n"
	);
#ifdef HAVE_ZLIB
	PUTS(
		"  DOCX        docx                *      *    .docx           [zlib]\n"
		"  ODT         odt                        *    .odt            [zlib]\n"
		"  EPUB        epub                       *    .epub           [zlib]\n"
	);
#endif
	PUTS(
		"  Plain Text  txt, text                  *    .txt\n"
		"  RTF         rtf                        *    .rtf\n"
		"  LaTeX       tex, latex                 *    .tex, .latex\n"
		"  PDF         pdf                        *    .pdf\n"
		"  JSON        json                       *    .json\n"
		"  RST         rst                        *    .rst\n"
		"  AsciiDoc    adoc, asciidoc             *    .adoc, .asciidoc\n"
		"  Org Mode    org                        *    .org\n"
		"  Textile     textile                    *    .textile\n"
		"  MediaWiki   mediawiki, wiki            *    .mediawiki\n"
		"  Creole      creole                     *    .creole\n"
		"  DokuWiki    dokuwiki                   *    .dokuwiki\n"
		"  Jira        jira, confluence           *    .jira\n"
		"  BBCode      bbcode                     *    .bbcode\n"
		"  Gemtext     gemtext, gmi               *    .gmi\n"
		"  Djot        djot                       *    .djot\n"
		"  Man/Troff   man, troff, groff          *    .1 - .9\n"
		"  Texinfo     texinfo, texi              *    .texi, .texinfo\n"
		"  POD         pod                        *    .pod\n"
		"  DocBook     docbook, xml               *    .xml\n"
		"  Typst       typst, typ                 *    .typ\n"
		"  OPML        opml                       *    .opml\n"
		"  Muse        muse                       *    .muse\n"
		"  Email       eml, email          *           .eml\n"
		"\n"
		"Formats are auto-detected from file extension if not specified.\n"
		"Text output goes to stdout; binary formats require -o/--output.\n"
	);
#ifndef HAVE_ZLIB
	PUTS("Note: DOCX/ODT/EPUB disabled (compile with -DHAVE_ZLIB -lz to enable)\n");
#endif
}

int main(int argc, char **argv)
{
	arena_init(64 * 1024);  // 64KB initial size

	struct option options[] = {
		{ "from", required_argument, 0, 'f' },
		{ "to", required_argument, 0, 't' },
		{ "output", required_argument, 0, 'o' },
		{ "max-bytes", required_argument, 0, 'B' },
		{ "strict", no_argument, 0, 'S' },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	enum format from_fmt = FMT_UNKNOWN;
	enum format to_fmt = FMT_UNKNOWN;
	const char *output_path = NULL;

	int c;
	while ((c = getopt_long(argc, argv, "f:t:o:B:Sh", options, NULL)) != -1) {
		switch (c) {
			case 'f':
				from_fmt = format_from_name(optarg);
				if (from_fmt == FMT_UNKNOWN) {
					PRINTF_ERR("Unknown input format: %s\n", optarg);
					return EXIT_FAILURE;
				}
				break;
			case 't':
				to_fmt = format_from_name(optarg);
				if (to_fmt == FMT_UNKNOWN) {
					PRINTF_ERR("Unknown output format: %s\n", optarg);
					return EXIT_FAILURE;
					}
					break;
				case 'B': {
					char *end;
					errno = 0;
					unsigned long long val = strtoull(optarg, &end, 10);
					if (errno != 0 || end == optarg || *end != '\0' || optarg[0] == '-' || val > SIZE_MAX) {
						PRINTF_ERR("Invalid max-bytes: %s\n", optarg);
						return EXIT_FAILURE;
					}
					max_bytes = (size_t)val;
					break;
				}
				case 'o':
					output_path = optarg;
					break;
				case 'S':
					strict_mode = true;
				break;
			case 'h':
				print_help();
				return EXIT_SUCCESS;
			case '?':
				return EXIT_FAILURE;
		}
	}

	int positional_count = argc - optind;
	if (positional_count <= 0) {
		PUTS_ERR("Error: no input file specified\n");
		print_help();
		return EXIT_FAILURE;
	}
	if ((output_path && positional_count > 1) || (!output_path && positional_count > 2)) {
		PUTS_ERR("Error: too many positional arguments\n");
		print_help();
		return EXIT_FAILURE;
	}

	const char *input_path = argv[optind];
	if (!output_path && positional_count == 2) {
		output_path = argv[optind + 1];
	}

	int exit_code = EXIT_FAILURE;
	size_t input_len = 0;
	char *input_data = NULL;
	char *output_data = NULL;
	size_t output_len = 0;
	bool is_binary = false;

	// Read input
	errno = 0;
	input_data = read_file(input_path, &input_len);
	int read_errno = errno;
	if (!input_data) {
		if (read_errno == EFBIG) {
			PRINTF_ERR("Error: input file exceeds --max-bytes limit (%zu bytes)\n", max_bytes);
			goto cleanup;
		}
		PRINTF_ERR("Error: cannot read %s\n", input_path);
		goto cleanup;
	}

	// Detect/validate formats
	if (from_fmt == FMT_UNKNOWN) {
		from_fmt = detect_format(input_path, input_data, input_len);
	}

	if (to_fmt == FMT_UNKNOWN && output_path) {
		to_fmt = detect_format(output_path, NULL, 0);
	}

	if (to_fmt == FMT_UNKNOWN) {
		PUTS_ERR("Error: cannot determine output format, use -t\n");
		goto cleanup;
	}

	if (strict_mode && from_fmt == FMT_MARKDOWN) {
		if (strcasestr(input_data, "<table") != NULL) {
			PUTS_ERR("Error: HTML tables in Markdown are not supported\n");
			goto cleanup;
		}
	}

	// Check for write-only formats used as input
	switch (from_fmt) {
		case FMT_PDF:
		case FMT_TEXT:
		case FMT_RTF:
		case FMT_LATEX:
		case FMT_ODT:
		case FMT_EPUB:
		case FMT_JSON:
		case FMT_RST:
		case FMT_ASCIIDOC:
		case FMT_ORG:
		case FMT_TEXTILE:
		case FMT_MEDIAWIKI:
		case FMT_CREOLE:
		case FMT_DOKUWIKI:
		case FMT_JIRA:
		case FMT_BBCODE:
		case FMT_GEMTEXT:
		case FMT_DJOT:
		case FMT_MAN:
		case FMT_TEXINFO:
		case FMT_POD:
		case FMT_DOCBOOK:
		case FMT_TYPST:
		case FMT_OPML:
		case FMT_MUSE:
			PUTS_ERR("Error: reading this format is not supported\n");
			goto cleanup;
#ifndef HAVE_ZLIB
		case FMT_DOCX:
			PUTS_ERR("Error: DOCX support requires zlib (compile with -DHAVE_ZLIB -lz)\n");
			goto cleanup;
#endif
		default:
			break;
	}

#ifndef HAVE_ZLIB
	// Check for zlib-dependent output formats
	if (to_fmt == FMT_DOCX || to_fmt == FMT_ODT || to_fmt == FMT_EPUB) {
		PUTS_ERR("Error: DOCX/ODT/EPUB support requires zlib (compile with -DHAVE_ZLIB -lz)\n");
		goto cleanup;
	}
#endif

	// Parse input
	struct document *doc = NULL;
	switch (from_fmt) {
		case FMT_MARKDOWN:
			doc = parse_markdown(input_data, input_len);
			break;
		case FMT_HTML:
			doc = parse_html(input_data, input_len);
			break;
#ifdef HAVE_ZLIB
		case FMT_DOCX:
			doc = parse_docx(input_data, input_len);
			break;
#endif
		case FMT_EML:
			doc = parse_eml(input_data, input_len);
			break;
		default:
			break;
	}

	if (!doc) {
		PUTS_ERR("Error: failed to parse input\n");
		goto cleanup;
	}

	bool output_supports_tables =
		to_fmt == FMT_MARKDOWN ||
		to_fmt == FMT_HTML ||
		to_fmt == FMT_JSON
	#ifdef HAVE_ZLIB
		|| to_fmt == FMT_DOCX
	#endif
		;

	bool has_tables = doc_contains_tables(doc->blocks);
	if (has_tables) {
		if (strict_mode && !output_supports_tables) {
			PUTS_ERR("Error: tables are not supported for this output format\n");
			goto cleanup;
		}
		if (!strict_mode && !output_supports_tables) {
			flatten_tables_in_blocks(&doc->blocks);
		}
	}

	// Check for write-only output format requested as input
	if (to_fmt == FMT_EML) {
		PUTS_ERR("Error: writing EML format is not supported\n");
		goto cleanup;
	}

	// Write output
	switch (to_fmt) {
#ifdef HAVE_ZLIB
		case FMT_DOCX:
		case FMT_ODT:
		case FMT_EPUB:
			is_binary = true;
			break;
#endif
		case FMT_PDF:
			is_binary = true;
			break;
		default:
			break;
	}

	if (output_path && !is_binary) {
		if (!write_stream_output(output_path, doc, to_fmt)) {
			PRINTF_ERR("Error: cannot write %s\n", output_path);
			goto cleanup;
		}
		exit_code = EXIT_SUCCESS;
		goto cleanup;
	}

	switch (to_fmt) {
		case FMT_MARKDOWN:
			output_data = write_markdown(doc);
			output_len = strlen(output_data);
			break;
		case FMT_HTML:
			output_data = write_html(doc);
			output_len = strlen(output_data);
			break;
#ifdef HAVE_ZLIB
		case FMT_DOCX:
			output_data = write_docx(doc, &output_len);
			is_binary = true;
			break;
		case FMT_ODT:
			output_data = write_odt(doc, &output_len);
			is_binary = true;
			break;
		case FMT_EPUB:
			output_data = write_epub(doc, &output_len);
			is_binary = true;
			break;
#endif
		case FMT_PDF:
			output_data = write_pdf(doc, &output_len);
			is_binary = true;
			break;
		case FMT_TEXT:
			output_data = write_text(doc);
			output_len = strlen(output_data);
			break;
		case FMT_RTF:
			output_data = write_rtf(doc);
			output_len = strlen(output_data);
			break;
		case FMT_LATEX:
			output_data = write_latex(doc);
			output_len = strlen(output_data);
			break;
		case FMT_JSON:
			output_data = write_json(doc);
			output_len = strlen(output_data);
			break;
		case FMT_RST:
			output_data = write_rst(doc);
			output_len = strlen(output_data);
			break;
		case FMT_ASCIIDOC:
			output_data = write_asciidoc(doc);
			output_len = strlen(output_data);
			break;
		case FMT_ORG:
			output_data = write_org(doc);
			output_len = strlen(output_data);
			break;
		case FMT_TEXTILE:
			output_data = write_textile(doc);
			output_len = strlen(output_data);
			break;
		case FMT_MEDIAWIKI:
			output_data = write_mediawiki(doc);
			output_len = strlen(output_data);
			break;
		case FMT_CREOLE:
			output_data = write_creole(doc);
			output_len = strlen(output_data);
			break;
		case FMT_DOKUWIKI:
			output_data = write_dokuwiki(doc);
			output_len = strlen(output_data);
			break;
		case FMT_JIRA:
			output_data = write_jira(doc);
			output_len = strlen(output_data);
			break;
		case FMT_BBCODE:
			output_data = write_bbcode(doc);
			output_len = strlen(output_data);
			break;
		case FMT_GEMTEXT:
			output_data = write_gemtext(doc);
			output_len = strlen(output_data);
			break;
		case FMT_DJOT:
			output_data = write_djot(doc);
			output_len = strlen(output_data);
			break;
		case FMT_MAN:
			output_data = write_man(doc);
			output_len = strlen(output_data);
			break;
		case FMT_TEXINFO:
			output_data = write_texinfo(doc);
			output_len = strlen(output_data);
			break;
		case FMT_POD:
			output_data = write_pod(doc);
			output_len = strlen(output_data);
			break;
		case FMT_DOCBOOK:
			output_data = write_docbook(doc);
			output_len = strlen(output_data);
			break;
		case FMT_TYPST:
			output_data = write_typst(doc);
			output_len = strlen(output_data);
			break;
		case FMT_OPML:
			output_data = write_opml(doc);
			output_len = strlen(output_data);
			break;
		case FMT_MUSE:
			output_data = write_muse(doc);
			output_len = strlen(output_data);
			break;
		default:
			break;
	}

	if (!output_data) {
		PUTS_ERR("Error: failed to generate output\n");
		goto cleanup;
	}

	// Write to file or stdout
	bool success;
	if (output_path) {
		success = write_file(output_path, output_data, output_len);
		if (!success) {
			PRINTF_ERR("Error: cannot write %s\n", output_path);
		}
	} else if (is_binary) {
		PUTS_ERR("Error: binary output requires -o filename\n");
		success = false;
	} else {
		WRITE(output_data, output_len);
		success = true;
	}

	exit_code = success ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
	free(output_data);
	free(input_data);
	return exit_code;
}
