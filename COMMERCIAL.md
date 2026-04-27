# Commercial Licensing

## Build Products That Last

AkiraOS is a production-grade embedded OS built on Zephyr RTOS, designed for teams who need more than a prototype — real hardware, real deployments, real scale. It brings a WebAssembly application runtime, over-the-air update infrastructure, hardware abstraction across dozens of boards, and a full connectivity stack to your firmware, so your team can focus on the product rather than the platform.

AkiraOS is **open source under the Apache License 2.0**, which means you can evaluate it, build with it, and ship open-source products at no cost. For commercial products that need enterprise support, the AkiraHub managed platform, or simplified compliance, a commercial license is the right path.

---

## Why AkiraOS

**Ship faster.** A WASM runtime means your application logic is decoupled from firmware. Update app logic over the air — no reflashing. Your release cadence is no longer gated by firmware deployment windows.

**Run anywhere.** Supported hardware spans ESP32 variants, Nordic nRF54, STM32, and native simulation, with a board overlay system designed for easy porting. One codebase, many targets.

**Stay secure.** Built-in NVS encryption, MCUboot secure boot, PSA Certified-aligned security model, and a structured module isolation architecture — security is designed in, not bolted on.

**Own your stack.** AkiraOS is not a cloud-locked SDK. Your firmware runs on your hardware. AkiraHub is an optional managed layer, not a dependency.

---

## Licensing Tiers

### Community — Free (Apache 2.0)

The full AkiraOS platform, no restrictions, no fees. Start building today.

- Complete AkiraOS runtime and WASM API surface
- AkiraSDK with production-ready sample applications
- All supported hardware targets
- Community support via GitHub Issues and Discussions
- No device limits, no license keys

**Ideal for:** makers, students, open-source projects, and teams evaluating the platform.

---

### Startup OEM

For small hardware companies taking their first product to market.

- **Commercial license** — removes Apache 2.0 attribution obligations from your firmware distribution
- Access to **AkiraHub** hosted SaaS — device registry, app catalogue, remote install
- `akira-cli` signing service — sign and distribute `.akpkg` apps to your fleet
- Email support with a defined response SLO
- One product line definition

**Ideal for:** hardware startups shipping their first OEM product.

> Pricing on request — [contact us](mailto:enterprise@pen.engineering)

---

### Professional OEM

For established hardware companies with active deployments and growing teams.

Everything in Startup, plus:

- Unlimited devices under one product line
- App signing portal — publish apps to your own branded store
- Priority support with faster response SLO
- One BSP porting consultation session included
- Compliance documentation pack (security model, SBOM export)
- Quarterly roadmap preview calls

**Ideal for:** hardware companies with production deployments who need scalability and compliance support.

> Pricing on request — [contact us](mailto:enterprise@pen.engineering)

---

### Enterprise

For industrial, medical, automotive, and large-scale IoT deployments where reliability, compliance, and control are non-negotiable.

Everything in Professional, plus:

- **Self-hosted AkiraHub** — on-premise or private cloud deployment, fully under your control
- Advanced security module — NVS encryption, HSM/secure element integration
- Full compliance documentation — PSA Certified, IEC 62443, CycloneDX SBOM
- White-label SDK — rename and rebrand the API surface for your own developer program
- SLA-backed support: critical response within hours, dedicated escalation path
- Dedicated Slack channel with direct engineering access
- Custom feature and driver development (time and materials)
- Patent license and legal indemnification

**Ideal for:** regulated industries, large IoT fleet operators, and teams building critical infrastructure.

> Pricing on request — [contact us](mailto:enterprise@pen.engineering)

---

## Professional Services

We work directly with your engineering team on scoped engagements:

| Service | Description |
|---------|-------------|
| **BSP Porting** | Bring AkiraOS to your custom hardware or unsupported SoC |
| **Custom Driver Development** | Hardware-specific drivers integrated into the AkiraOS module system |
| **PSA Certified Preparation** | Consulting and documentation to support your certification process |
| **On-Site Integration Support** | Hands-on engineering support at your facility |
| **Security Review** | Threat model review and security hardening for your firmware configuration |

All services are scoped and quoted per engagement. [Reach out](mailto:enterprise@pen.engineering) to discuss your requirements.

---

## Frequently Asked Questions

**Do I need a commercial license to sell a product that runs AkiraOS?**

No — if you comply with the Apache 2.0 attribution requirements (retain copyright notices and the `NOTICE` file), you can ship commercial products under the Community tier at no cost. A commercial license is for teams who need to remove those obligations, access AkiraHub, or require enterprise SLAs.

**Does Apache 2.0 require me to open-source my application code?**

No. Apache 2.0 is a permissive license — your WASM applications and product-specific code remain fully proprietary. Only modifications to AkiraOS itself must be attributed.

**What is a "product line"?**

A product line is a single firmware configuration targeting a family of hardware variants sharing the same AkiraOS build (`prj.conf` + board overlays). Multiple SKUs of the same product count as one product line.

**Can I evaluate commercial features before committing?**

Yes. Contact us for a time-limited evaluation of AkiraHub and the enterprise security modules — no commitment required.

**How does pricing work for large device fleets?**

Pricing is negotiated based on your fleet size, support requirements, and contract structure. We do not publish fixed per-device rates — contact us for a conversation.

**Can AkiraOS run without AkiraHub?**

Yes. AkiraHub is an optional managed layer for device registry, remote app deployment, and signing. AkiraOS runs fully standalone — you own your stack.

---

## Contact

To discuss licensing, services, or a trial:

**Email:** enterprise@pen.engineering  
**Website:** [akiraos.dev/enterprise](https://akiraos.dev/enterprise)  
**Company:** PenEngineering S.R.L

We respond within one business day.

---

## Third-Party Components

AkiraOS incorporates the following Apache 2.0 licensed components:

- [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr) — Apache 2.0
- [WASM Micro Runtime (WAMR)](https://github.com/bytecodealliance/wasm-micro-runtime) — Apache 2.0
- [MCUboot](https://github.com/mcu-tools/mcuboot) — Apache 2.0

These components retain their own copyright notices as required by their licenses.
