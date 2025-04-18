#pragma once

#include <QString>

struct SmilePlotData {
    QString symbol; // e.g., "AAPL2025-04-17100.0p"
    double strike = 0.0;
    double mid_iv = 0.0;
    double theo_iv = 0.0;
    double bid_iv = 0.0;
    double ask_iv = 0.0;
    double bid_price = 0.0;
    double ask_price = 0.0;

    // Helper to format data for tooltip
    QString formatForTooltip() const {
        // Basic formatting, can use HTML for more richness
        return QString("Symbol: %1\nStrike: %2\nMid IV: %3\nTheo IV: %4\nBid/Ask IV: %5 / %6\nBid/Ask $: %7 / %8")
            .arg(symbol)
            .arg(strike, 0, 'f', 2) // Format double with 2 decimal places
            .arg(mid_iv, 0, 'f', 4)
            .arg(theo_iv, 0, 'f', 4)
            .arg(bid_iv, 0, 'f', 4)
            .arg(ask_iv, 0, 'f', 4)
            .arg(bid_price, 0, 'f', 2)
            .arg(ask_price, 0, 'f', 2);
        // Add other fields here
    }
};

// Make it known to the meta-object system if passing via signals/slots (optional here)
// Q_DECLARE_METATYPE(SmilePointData)
