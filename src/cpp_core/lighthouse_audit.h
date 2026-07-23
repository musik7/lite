#pragma once
#include "core.h"

// ============ LIGHTHOUSE CATEGORY ENUM ============
enum class AuditCategory {
    Performance,
    Accessibility,
    BestPractices,
    SEO,
    PWA
};

enum class AuditStatus {
    Passed,
    Warning,
    Failed,
    Informational
};

// ============ AUDIT DIAGNOSTIC ITEM ============
struct AuditDiagnostic {
    StringView audit_id;
    StringView title;
    StringView description;
    AuditCategory category;
    AuditStatus status;
    int score;                      // 0 - 100
    double estimated_savings_ms;    // Time savings potential
    size_t estimated_savings_bytes; // Network transfer savings
    StringView snippet_or_url;
    AuditDiagnostic* next;
};

// ============ CORE WEB VITALS METRICS ============
struct CoreWebVitals {
    double fcp_ms;                  // First Contentful Paint (ms)
    double lcp_ms;                  // Largest Contentful Paint (ms)
    double tbt_ms;                  // Total Blocking Time (ms)
    double cls_score;               // Cumulative Layout Shift (0.0 - 1.0)
    double speed_index_ms;          // Speed Index (ms)
    double inp_ms;                  // Interaction to Next Paint (ms)

    int get_fcp_score() const { return fcp_ms <= 1800 ? 100 : (fcp_ms <= 3000 ? 65 : 20); }
    int get_lcp_score() const { return lcp_ms <= 2500 ? 100 : (lcp_ms <= 4000 ? 65 : 15); }
    int get_tbt_score() const { return tbt_ms <= 200 ? 100 : (tbt_ms <= 600 ? 60 : 10); }
    int get_cls_score() const { return cls_score <= 0.10 ? 100 : (cls_score <= 0.25 ? 65 : 10); }
    int get_speed_index_score() const { return speed_index_ms <= 3400 ? 100 : (speed_index_ms <= 5800 ? 65 : 20); }
    int get_inp_score() const { return inp_ms <= 200 ? 100 : (inp_ms <= 500 ? 60 : 15); }

    int compute_overall_performance_score() const {
        // Weighted Core Web Vitals formula matching Lighthouse standard
        double weighted = (get_fcp_score() * 0.10) +
                          (get_lcp_score() * 0.25) +
                          (get_tbt_score() * 0.30) +
                          (get_cls_score() * 0.25) +
                          (get_speed_index_score() * 0.10);
        return (int)weighted;
    }
};

// ============ LIGHTHOUSE AUDIT REPORT ============
struct LighthouseReport {
    StringView url;
    double timestamp;
    
    // Category Scores (0 - 100)
    int performance_score;
    int accessibility_score;
    int best_practices_score;
    int seo_score;
    int pwa_score;

    CoreWebVitals web_vitals;

    AuditDiagnostic* first_diagnostic;
    size_t diagnostic_count;
    size_t passed_audits_count;

    void add_diagnostic(ArenaAllocator* arena, StringView id, StringView title, StringView desc, AuditCategory cat, AuditStatus status, int score, double savings_ms, size_t savings_bytes, StringView snippet = StringView("")) {
        AuditDiagnostic* diag = (AuditDiagnostic*)arena->allocate(sizeof(AuditDiagnostic));
        diag->audit_id = copy_string(arena, id);
        diag->title = copy_string(arena, title);
        diag->description = copy_string(arena, desc);
        diag->category = cat;
        diag->status = status;
        diag->score = score;
        diag->estimated_savings_ms = savings_ms;
        diag->estimated_savings_bytes = savings_bytes;
        diag->snippet_or_url = copy_string(arena, snippet);
        diag->next = first_diagnostic;
        first_diagnostic = diag;
        diagnostic_count++;

        if (status == AuditStatus::Passed || score >= 90) {
            passed_audits_count++;
        }
    }
};

// ============ LIGHTHOUSE AUDIT ENGINE ============
class LighthouseAuditEngine {
private:
    ArenaAllocator* arena;

public:
    LighthouseAuditEngine(ArenaAllocator* a) : arena(a) {}

    LighthouseReport* run_audit(StringView page_url, CoreWebVitals vitals) {
        LighthouseReport* report = (LighthouseReport*)arena->allocate(sizeof(LighthouseReport));
        report->url = copy_string(arena, page_url);
        report->timestamp = 1700000000.0;
        report->web_vitals = vitals;
        report->first_diagnostic = nullptr;
        report->diagnostic_count = 0;
        report->passed_audits_count = 0;

        // 1. Compute Performance Score from Web Vitals
        report->performance_score = vitals.compute_overall_performance_score();

        // 2. Add Performance Audits
        if (vitals.lcp_ms > 2500) {
            report->add_diagnostic(arena, StringView("unused-css-rules"), StringView("Reduce Unused CSS"), StringView("Defer or remove unused CSS rules to reduce bytes consumed by network activity."), AuditCategory::Performance, AuditStatus::Warning, 62, 340.0, 142000, StringView("styles/main.css"));
        } else {
            report->add_diagnostic(arena, StringView("unused-css-rules"), StringView("Reduce Unused CSS"), StringView("All CSS loaded on page is actively utilized."), AuditCategory::Performance, AuditStatus::Passed, 100, 0.0, 0);
        }

        if (vitals.tbt_ms > 200) {
            report->add_diagnostic(arena, StringView("render-blocking-resources"), StringView("Eliminate Render-Blocking Resources"), StringView("Resources are blocking the first paint of your page. Consider delivering critical JS/CSS inline."), AuditCategory::Performance, AuditStatus::Failed, 35, 520.0, 210000, StringView("vendor/bundle.js"));
        }

        report->add_diagnostic(arena, StringView("uses-optimized-images"), StringView("Serve Images in Next-Gen Formats"), StringView("WebP and AVIF image formats provide better compression than PNG or JPEG."), AuditCategory::Performance, AuditStatus::Warning, 78, 180.0, 480000, StringView("hero-banner.png"));

        // 3. Accessibility Audits
        report->accessibility_score = 92;
        report->add_diagnostic(arena, StringView("color-contrast"), StringView("Background and Foreground Colors Have Sufficient Contrast"), StringView("High-contrast text helps low-vision users read content easily."), AuditCategory::Accessibility, AuditStatus::Passed, 100, 0.0, 0);
        report->add_diagnostic(arena, StringView("aria-allowed-attr"), StringView("[aria-*] attributes match their roles"), StringView("Elements specify appropriate ARIA attribute configurations."), AuditCategory::Accessibility, AuditStatus::Passed, 100, 0.0, 0);
        report->add_diagnostic(arena, StringView("button-name"), StringView("Buttons do not have an accessible name"), StringView("When a button has no text content, screen readers announce it as 'button' without context."), AuditCategory::Accessibility, AuditStatus::Failed, 40, 0.0, 0, StringView("<button class='icon-btn'></button>"));

        // 4. Best Practices Audits
        report->best_practices_score = 96;
        report->add_diagnostic(arena, StringView("is-on-https"), StringView("Uses HTTPS for secure transport"), StringView("All resources served over secure HTTPS connections."), AuditCategory::BestPractices, AuditStatus::Passed, 100, 0.0, 0);
        report->add_diagnostic(arena, StringView("no-document-write"), StringView("Avoids 'document.write()'"), StringView("For dynamically injected scripts, use asynchronous script creation instead."), AuditCategory::BestPractices, AuditStatus::Passed, 100, 0.0, 0);

        // 5. SEO Audits
        report->seo_score = 100;
        report->add_diagnostic(arena, StringView("viewport"), StringView("Has a '<meta name=\"viewport\">' tag with 'width' or 'initial-scale'"), StringView("Optimizes mobile screen rendering and font scaling."), AuditCategory::SEO, AuditStatus::Passed, 100, 0.0, 0);
        report->add_diagnostic(arena, StringView("document-title"), StringView("Document has a '<title>' element"), StringView("Title provides search engine crawlers with page context."), AuditCategory::SEO, AuditStatus::Passed, 100, 0.0, 0);

        // 6. PWA Audits
        report->pwa_score = 88;
        report->add_diagnostic(arena, StringView("service-worker"), StringView("Registers a Service Worker for offline capability"), StringView("Service worker handles offline request caching and background sync."), AuditCategory::PWA, AuditStatus::Passed, 100, 0.0, 0);
        report->add_diagnostic(arena, StringView("manifest-installable"), StringView("Web App Manifest meets installability requirements"), StringView("Includes name, icons, and start_url for PWA installation."), AuditCategory::PWA, AuditStatus::Passed, 100, 0.0, 0);

        return report;
    }
};
