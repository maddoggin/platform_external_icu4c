/*
*******************************************************************************
* Copyright (C) 2009-2011, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File PLURFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*******************************************************************************
*/

#include "unicode/messagepattern.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "unicode/utypes.h"
#include "cmemory.h"
#include "messageimpl.h"
#include "plurrule_impl.h"
#include "uassert.h"
#include "uhash.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

static const UChar OTHER_STRING[] = {
    0x6F, 0x74, 0x68, 0x65, 0x72, 0  // "other"
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralFormat)

PluralFormat::PluralFormat(UErrorCode& status)
        : locale(Locale::getDefault()),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(NULL, status);
}

PluralFormat::PluralFormat(const Locale& loc, UErrorCode& status)
        : locale(loc),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(NULL, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, UErrorCode& status)
        : locale(Locale::getDefault()),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(&rules, status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const PluralRules& rules,
                           UErrorCode& status)
        : locale(loc),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(&rules, status);
}

PluralFormat::PluralFormat(const UnicodeString& pat,
                           UErrorCode& status)
        : locale(Locale::getDefault()),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(NULL, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const UnicodeString& pat,
                           UErrorCode& status)
        : locale(loc),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(NULL, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralRules& rules,
                           const UnicodeString& pat,
                           UErrorCode& status)
        : locale(Locale::getDefault()),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(&rules, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const PluralRules& rules,
                           const UnicodeString& pat,
                           UErrorCode& status)
        : locale(loc),
          msgPattern(status),
          numberFormat(NULL),
          offset(0) {
    init(&rules, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralFormat& other)
        : Format(other),
          locale(other.locale),
          msgPattern(other.msgPattern),
          numberFormat(NULL),
          offset(other.offset) {
    copyObjects(other);
}

void
PluralFormat::copyObjects(const PluralFormat& other) {
    UErrorCode status = U_ZERO_ERROR;
    if (numberFormat != NULL) {
        delete numberFormat;
    }
    if (pluralRulesWrapper.pluralRules != NULL) {
        delete pluralRulesWrapper.pluralRules;
    }

    if (other.numberFormat == NULL) {
        numberFormat = NumberFormat::createInstance(locale, status);
    } else {
        numberFormat = (NumberFormat*)other.numberFormat->clone();
    }
    if (other.pluralRulesWrapper.pluralRules == NULL) {
        pluralRulesWrapper.pluralRules = PluralRules::forLocale(locale, status);
    } else {
        pluralRulesWrapper.pluralRules = other.pluralRulesWrapper.pluralRules->clone();
    }
}


PluralFormat::~PluralFormat() {
    delete numberFormat;
}

void
PluralFormat::init(const PluralRules* rules, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    if (rules==NULL) {
        pluralRulesWrapper.pluralRules = PluralRules::forLocale(locale, status);
    } else {
        pluralRulesWrapper.pluralRules = rules->clone();
        if (pluralRulesWrapper.pluralRules == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    numberFormat= NumberFormat::createInstance(locale, status);
}

void
PluralFormat::applyPattern(const UnicodeString& newPattern, UErrorCode& status) {
    msgPattern.parsePluralStyle(newPattern, NULL, status);
    if (U_FAILURE(status)) {
        msgPattern.clear();
        offset = 0;
        return;
    }
    offset = msgPattern.getPluralOffset(0);
}

UnicodeString&
PluralFormat::format(const Formattable& obj,
                   UnicodeString& appendTo,
                   FieldPosition& pos,
                   UErrorCode& status) const
{
    if (U_FAILURE(status)) return appendTo;

    if (obj.isNumeric()) {
        return format(obj.getDouble(), appendTo, pos, status);
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
}

UnicodeString
PluralFormat::format(int32_t number, UErrorCode& status) const {
    FieldPosition fpos(0);
    UnicodeString result;
    return format(number, result, fpos, status);
}

UnicodeString
PluralFormat::format(double number, UErrorCode& status) const {
    FieldPosition fpos(0);
    UnicodeString result;
    return format(number, result, fpos, status);
}


UnicodeString&
PluralFormat::format(int32_t number,
                     UnicodeString& appendTo,
                     FieldPosition& pos,
                     UErrorCode& status) const {
    return format((double)number, appendTo, pos, status);
}

UnicodeString&
PluralFormat::format(double number,
                     UnicodeString& appendTo,
                     FieldPosition& pos,
                     UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    if (msgPattern.countParts() == 0) {
        return numberFormat->format(number, appendTo, pos);
    }
    // Get the appropriate sub-message.
    int32_t partIndex = findSubMessage(msgPattern, 0, pluralRulesWrapper, number, status);
    // Replace syntactic # signs in the top level of this sub-message
    // (not in nested arguments) with the formatted number-offset.
    const UnicodeString& pattern = msgPattern.getPatternString();
    number -= offset;
    int32_t prevIndex = msgPattern.getPart(partIndex).getLimit();
    for (;;) {
        const MessagePattern::Part& part = msgPattern.getPart(++partIndex);
        const UMessagePatternPartType type = part.getType();
        int32_t index = part.getIndex();
        if (type == UMSGPAT_PART_TYPE_MSG_LIMIT) {
            return appendTo.append(pattern, prevIndex, index - prevIndex);
        } else if ((type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) ||
            (type == UMSGPAT_PART_TYPE_SKIP_SYNTAX && MessageImpl::jdkAposMode(msgPattern))) {
            appendTo.append(pattern, prevIndex, index - prevIndex);
            if (type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) {
                numberFormat->format(number, appendTo);
            }
            prevIndex = part.getLimit();
        } else if (type == UMSGPAT_PART_TYPE_ARG_START) {
            appendTo.append(pattern, prevIndex, index - prevIndex);
            prevIndex = index;
            partIndex = msgPattern.getLimitPartIndex(partIndex);
            index = msgPattern.getPart(partIndex).getLimit();
            MessageImpl::appendReducedApostrophes(pattern, prevIndex, index, appendTo);
            prevIndex = index;
        }
    }
}

UnicodeString&
PluralFormat::toPattern(UnicodeString& appendTo) {
    if (0 == msgPattern.countParts()) {
        appendTo.setToBogus();
    } else {
        appendTo.append(msgPattern.getPatternString());
    }
    return appendTo;
}

void
PluralFormat::setLocale(const Locale& loc, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    locale = loc;
    msgPattern.clear();
    delete numberFormat;
    offset = 0;
    numberFormat = NULL;
    pluralRulesWrapper.reset();
    init(NULL, status);
}

void
PluralFormat::setNumberFormat(const NumberFormat* format, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    NumberFormat* nf = (NumberFormat*)format->clone();
    if (nf != NULL) {
        delete numberFormat;
        numberFormat = nf;
    } else {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}

Format*
PluralFormat::clone() const
{
    return new PluralFormat(*this);
}


PluralFormat&
PluralFormat::operator=(const PluralFormat& other) {
    if (this != &other) {
        locale = other.locale;
        msgPattern = other.msgPattern;
        offset = other.offset;
        copyObjects(other);
    }

    return *this;
}

UBool
PluralFormat::operator==(const Format& other) const {
    if (this == &other) {
        return TRUE;
    }
    if (!Format::operator==(other)) {
        return FALSE;
    }
    const PluralFormat& o = (const PluralFormat&)other;
    return
        locale == o.locale &&
        msgPattern == o.msgPattern &&  // implies same offset
        (numberFormat == NULL) == (o.numberFormat == NULL) &&
        (numberFormat == NULL || *numberFormat == *o.numberFormat) &&
        (pluralRulesWrapper.pluralRules == NULL) == (o.pluralRulesWrapper.pluralRules == NULL) &&
        (pluralRulesWrapper.pluralRules == NULL ||
            *pluralRulesWrapper.pluralRules == *o.pluralRulesWrapper.pluralRules);
}

UBool
PluralFormat::operator!=(const Format& other) const {
    return  !operator==(other);
}

void
PluralFormat::parseObject(const UnicodeString& /*source*/,
                        Formattable& /*result*/,
                        ParsePosition& pos) const
{
    // Parsing not supported.
    pos.setErrorIndex(pos.getIndex());
}

int32_t PluralFormat::findSubMessage(const MessagePattern& pattern, int32_t partIndex,
                                     const PluralSelector& selector, double number, UErrorCode& ec) {
    if (U_FAILURE(ec)) {
        return 0;
    }
    int32_t count=pattern.countParts();
    double offset;
    const MessagePattern::Part* part=&pattern.getPart(partIndex);
    if (MessagePattern::Part::hasNumericValue(part->getType())) {
        offset=pattern.getNumericValue(*part);
        ++partIndex;
    } else {
        offset=0;
    }
    // The keyword is empty until we need to match against non-explicit, not-"other" value.
    // Then we get the keyword from the selector.
    // (In other words, we never call the selector if we match against an explicit value,
    // or if the only non-explicit keyword is "other".)
    UnicodeString keyword;
    UnicodeString other(FALSE, OTHER_STRING, 5);
    // When we find a match, we set msgStart>0 and also set this boolean to true
    // to avoid matching the keyword again (duplicates are allowed)
    // while we continue to look for an explicit-value match.
    UBool haveKeywordMatch=FALSE;
    // msgStart is 0 until we find any appropriate sub-message.
    // We remember the first "other" sub-message if we have not seen any
    // appropriate sub-message before.
    // We remember the first matching-keyword sub-message if we have not seen
    // one of those before.
    // (The parser allows [does not check for] duplicate keywords.
    // We just have to make sure to take the first one.)
    // We avoid matching the keyword twice by also setting haveKeywordMatch=true
    // at the first keyword match.
    // We keep going until we find an explicit-value match or reach the end of the plural style.
    int32_t msgStart=0;
    // Iterate over (ARG_SELECTOR [ARG_INT|ARG_DOUBLE] message) tuples
    // until ARG_LIMIT or end of plural-only pattern.
    do {
        part=&pattern.getPart(partIndex++);
        const UMessagePatternPartType type = part->getType();
        if(type==UMSGPAT_PART_TYPE_ARG_LIMIT) {
            break;
        }
        U_ASSERT (type==UMSGPAT_PART_TYPE_ARG_SELECTOR);
        // part is an ARG_SELECTOR followed by an optional explicit value, and then a message
        if(MessagePattern::Part::hasNumericValue(pattern.getPartType(partIndex))) {
            // explicit value like "=2"
            part=&pattern.getPart(partIndex++);
            if(number==pattern.getNumericValue(*part)) {
                // matches explicit value
                return partIndex;
            }
        } else if(!haveKeywordMatch) {
            // plural keyword like "few" or "other"
            // Compare "other" first and call the selector if this is not "other".
            if(pattern.partSubstringMatches(*part, other)) {
                if(msgStart==0) {
                    msgStart=partIndex;
                    if(0 == keyword.compare(other)) {
                        // This is the first "other" sub-message,
                        // and the selected keyword is also "other".
                        // Do not match "other" again.
                        haveKeywordMatch=TRUE;
                    }
                }
            } else {
                if(keyword.isEmpty()) {
                    keyword=selector.select(number-offset, ec);
                    if(msgStart!=0 && (0 == keyword.compare(other))) {
                        // We have already seen an "other" sub-message.
                        // Do not match "other" again.
                        haveKeywordMatch=TRUE;
                        // Skip keyword matching but do getLimitPartIndex().
                    }
                }
                if(!haveKeywordMatch && pattern.partSubstringMatches(*part, keyword)) {
                    // keyword matches
                    msgStart=partIndex;
                    // Do not match this keyword again.
                    haveKeywordMatch=TRUE;
                }
            }
        }
        partIndex=pattern.getLimitPartIndex(partIndex);
    } while(++partIndex<count);
    return msgStart;
}

PluralFormat::PluralSelectorAdapter::~PluralSelectorAdapter() {
    delete pluralRules;
}

UnicodeString PluralFormat::PluralSelectorAdapter::select(double number,
                                                          UErrorCode& /*ec*/) const {
    return pluralRules->select(number);
}

void PluralFormat::PluralSelectorAdapter::reset() {
    delete pluralRules;
    pluralRules = NULL;
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
