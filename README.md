# ore-xccy-curve-cpp

C++ XCCY Curve Builder with **full CrossCcyBasisMtMResetSwapHelper parameter support** (all 32 parameters).

The Python ORE SWIG binding only exposes 18 of 32 parameters. This package provides access to ALL parameters via pybind11, including:

- **Payment frequency** (`foreignTenor`, `domesticTenor`)
- **Payment lag** (`foreignPaymentLag`, `domesticPaymentLag`)
- **Fixing days** (`foreignFixingDays`, `domesticFixingDays`)
- **Lookback period** (`foreignLookback`, `domesticLookback`)
- **Rate cutoff** (`foreignRateCutoff`, `domesticRateCutoff`)
- **Include spread** (`foreignIncludeSpread`, `domesticIncludeSpread`)
- **Is averaged** (`foreignIsAveraged`, `domesticIsAveraged`)

## Prerequisites

Before installing, you need:

1. **ORE (Open Source Risk Engine)** - built and installed
2. **QuantLib** - usually bundled with ORE
3. **Boost** - required by QuantLib/ORE
4. **CMake** >= 3.15
5. **C++ compiler** - MSVC (Windows), GCC/Clang (Linux)
6. **Python** >= 3.8
7. **UV** - for installation

## Installation with UV

### 1. Set Environment Variables

Point to your ORE/QuantLib/Boost installations:

**Windows (cmd):**
```cmd
set ORE_ROOT=C:\path\to\ore
set QUANTLIB_ROOT=C:\path\to\quantlib
set BOOST_ROOT=C:\path\to\boost
```

**Windows (PowerShell):**
```powershell
$env:ORE_ROOT = "C:\path\to\ore"
$env:QUANTLIB_ROOT = "C:\path\to\quantlib"
$env:BOOST_ROOT = "C:\path\to\boost"
```

**Linux/macOS:**
```bash
export ORE_ROOT=/path/to/ore
export QUANTLIB_ROOT=/path/to/quantlib
export BOOST_ROOT=/path/to/boost
```

### 2. Install with UV

```bash
# Install from local path
uv pip install "c:\projects\ore-xccy-curve-cpp"

# Or if you're in the project directory
uv pip install .
```

### 3. Verify Installation

```python
python -c "import ore_xccy_curve_cpp; print('OK')"
```

## Quick Start

```python
from datetime import date
from ore_xccy_curve_cpp import (
    XCCYMarketDataCpp,
    XCCYConventionsCpp,
    XCCYCurveBuilderCpp,
    OISLegConventionsCpp,
    CurrencyConventionsCpp,
    create_flat_curve,
)

# 1. Create market data (pure quotes)
market_data = XCCYMarketDataCpp(
    valuation_date=date(2024, 1, 15),
    domestic_ccy="USD",
    foreign_ccy="GBP",
    fx_spot=1.2750,
    fx_forwards=[
        ("1M", -10.0),   # (tenor, forward_points in pips)
        ("3M", -30.0),
        ("6M", -55.0),
        ("1Y", -95.0),
    ],
    xccy_basis_swaps=[
        ("2Y", -12.5),   # (tenor, basis_spread in bps)
        ("5Y", -16.5),
        ("10Y", -19.5),
        ("30Y", -18.0),
    ],
    fx_base_ccy="GBP",
)

# 2. Create conventions with FULL parameter control
usd_leg = OISLegConventionsCpp(
    payment_tenor="3M",      # Quarterly payments
    payment_lag=2,           # 2 business day payment lag
    fixing_days=2,           # 2 days fixing offset
    rate_cutoff=2,           # 2 day rate cutoff
)
gbp_leg = OISLegConventionsCpp(
    payment_tenor="3M",
    payment_lag=0,           # No payment lag for SONIA
    fixing_days=0,
)

conventions = XCCYConventionsCpp(
    domestic=CurrencyConventionsCpp("USD", "SOFR", "US-FederalReserve", "Actual360"),
    foreign=CurrencyConventionsCpp("GBP", "SONIA", "UK-Exchange", "Actual365Fixed"),
    domestic_leg=usd_leg,
    foreign_leg=gbp_leg,
    settlement_days=2,
)

# Or use factory with standard defaults:
# conventions = XCCYConventionsCpp.from_currencies("USD", "GBP")

# 3. Create input curves (flat curves for demo)
val_date = date(2024, 1, 15)
usd_curve = create_flat_curve(val_date.year, val_date.month, val_date.day, 0.0525)
gbp_curve = create_flat_curve(val_date.year, val_date.month, val_date.day, 0.0475)

# 4. Build XCCY curve
builder = XCCYCurveBuilderCpp(
    market_data,
    conventions,
    domestic_discount_curve=usd_curve,
    domestic_index_curve=usd_curve,
    foreign_index_curve=gbp_curve,
)
builder.build()
builder.print_curve_summary()

# 5. Get curve values
target = date(2029, 1, 15)  # 5Y
print(f"5Y DF: {builder.get_discount_factor(target):.10f}")
print(f"5Y Zero: {builder.get_zero_rate(target) * 100:.4f}%")
print(f"5Y FX Fwd: {builder.get_implied_fx_forward(target):.4f}")
```

## Using with ORE Python Curves

You can pass ORE Python (SWIG) curves directly:

```python
import ORE as ore
from ore_xccy_curve_cpp import XCCYCurveBuilderCpp, XCCYConventionsCpp

# Your existing ORE curves
usd_ois_curve = ...  # ore.YieldTermStructureHandle
gbp_ois_curve = ...  # ore.YieldTermStructureHandle

# Build XCCY curve using C++ backend
builder = XCCYCurveBuilderCpp(
    market_data,
    conventions,
    domestic_discount_curve=usd_ois_curve,  # ORE curves work directly
    domestic_index_curve=usd_ois_curve,
    foreign_index_curve=gbp_ois_curve,
)
builder.build()
```

## API Reference

### Market Data

```python
XCCYMarketDataCpp(
    valuation_date: date,
    domestic_ccy: str,           # "USD"
    foreign_ccy: str,            # "GBP"
    fx_spot: float,              # 1.2750
    fx_forwards: List[Tuple[str, float]],      # [("1M", -10.0), ...]
    xccy_basis_swaps: List[Tuple[str, float]], # [("2Y", -12.5), ...]
    fx_base_ccy: Optional[str],  # "GBP" (which ccy is base in FX quote)
)
```

### Conventions

```python
OISLegConventionsCpp(
    payment_tenor: str = "3M",       # Payment frequency
    payment_lag: int = 2,            # Business days after period end
    fixing_days: int = 2,            # Observation offset
    lookback: Optional[str] = None,  # e.g., "2D" for 2-day lookback
    rate_cutoff: int = 2,            # Days before period end
    include_spread: bool = False,    # Include spread in compounding
    is_averaged: bool = False,       # Averaged vs compounded
)

CurrencyConventionsCpp(
    ccy: str,                        # "USD"
    ois_index_name: str,             # "SOFR", "SONIA", "ESTR", etc.
    calendar_name: str,              # "US-FederalReserve", "UK-Exchange", etc.
    day_count: str = "Actual360",    # "Actual360", "Actual365Fixed", etc.
)

XCCYConventionsCpp(
    domestic: CurrencyConventionsCpp,
    foreign: CurrencyConventionsCpp,
    domestic_leg: OISLegConventionsCpp,
    foreign_leg: OISLegConventionsCpp,
    settlement_days: int = 2,
)

# Factory method for standard conventions:
XCCYConventionsCpp.from_currencies("USD", "GBP")
```

### Supported OIS Indices

- `SOFR` (USD)
- `SONIA` (GBP)
- `ESTR` (EUR)
- `TONAR` (JPY)
- `SARON` (CHF)
- `AONIA` (AUD)
- `CORRA` (CAD)

### Supported Calendars

- `US-FederalReserve`, `US-NYSE`
- `UK-Exchange`
- `TARGET`
- `Japan`
- `Switzerland`
- `Australia`
- `Canada`

### Supported Day Counts

- `Actual360`
- `Actual365Fixed`
- `ActualActual`
- `Thirty360`

## Comparison: Python SWIG vs C++ pybind11

| Parameter | Python SWIG (18) | C++ pybind11 (32) |
|-----------|------------------|-------------------|
| spreadQuote | ✓ | ✓ |
| spotFX | ✓ | ✓ |
| settlementDays | ✓ | ✓ |
| settlementCalendar | ✓ | ✓ |
| swapTenor | ✓ | ✓ |
| rollConvention | ✓ | ✓ |
| foreignCcyIndex | ✓ | ✓ |
| domesticCcyIndex | ✓ | ✓ |
| foreignCcyDiscountCurve | ✓ | ✓ |
| domesticCcyDiscountCurve | ✓ | ✓ |
| foreignIndexGiven | ✓ | ✓ |
| domesticIndexGiven | ✓ | ✓ |
| foreignDiscountCurveGiven | ✓ | ✓ |
| domesticDiscountCurveGiven | ✓ | ✓ |
| foreignCcyFxFwdRateCurve | ✓ | ✓ |
| domesticCcyFxFwdRateCurve | ✓ | ✓ |
| eom | ✓ | ✓ |
| spreadOnForeignCcy | ✓ | ✓ |
| **foreignTenor** | ✗ | ✓ |
| **domesticTenor** | ✗ | ✓ |
| **foreignPaymentLag** | ✗ | ✓ |
| **domesticPaymentLag** | ✗ | ✓ |
| **foreignIncludeSpread** | ✗ | ✓ |
| **foreignLookback** | ✗ | ✓ |
| **foreignFixingDays** | ✗ | ✓ |
| **foreignRateCutoff** | ✗ | ✓ |
| **foreignIsAveraged** | ✗ | ✓ |
| **domesticIncludeSpread** | ✗ | ✓ |
| **domesticLookback** | ✗ | ✓ |
| **domesticFixingDays** | ✗ | ✓ |
| **domesticRateCutoff** | ✗ | ✓ |
| **domesticIsAveraged** | ✗ | ✓ |
| telescopicValueDates | ✗ | ✓ |
| pillarChoice | ✗ | ✓ |

## Troubleshooting

### CMake can't find ORE/QuantLib/Boost

Ensure environment variables are set correctly:
```bash
echo %ORE_ROOT%        # Windows cmd
echo $ORE_ROOT         # Linux/macOS
```

### Linker errors on Windows

Add ORE/QuantLib library paths to your PATH:
```cmd
set PATH=%ORE_ROOT%\lib;%PATH%
```

### Import error after installation

Check that the compiled module matches your Python version:
```python
import sys
print(sys.version)  # Should match the Python used during build
```

## License

MIT License