// license:GPL-2.0+
// copyright-holders:Couriersud

#ifndef NLID_TWOTERM_H_
#define NLID_TWOTERM_H_

///
/// \file nlid_twoterm.h
///
/// Devices with two terminals ...
///
///
///       (k)
///  +-----T-----+
///  |     |     |
///  |  +--+--+  |
///  |  |     |  |
///  |  R     |  |
///  |  R     |  |
///  |  R     I  |
///  |  |     I  |  Device n
///  |  V+    I  |
///  |  V     |  |
///  |  V-    |  |
///  |  |     |  |
///  |  +--+--+  |
///  |     |     |
///  +-----T-----+
///       (l)
///
///  This is a resistance in series to a voltage source and paralleled by a
///  current source. This is suitable to model voltage sources, current sources,
///  resistors, capacitors, inductances and diodes.
///
////

#include "netlist/nl_base.h"
#include "netlist/nl_setup.h"
#include "netlist/plib/pfunction.h"
#include "netlist/solver/nld_solver.h"
#include "nld_generic_models.h"

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

namespace netlist
{
namespace analog
{

	// -----------------------------------------------------------------------------
	// nld_twoterm
	// -----------------------------------------------------------------------------

	template <class C>
	inline core_device_t &bselect(bool b, C &d1, core_device_t &d2)
	{
		auto *h = dynamic_cast<core_device_t *>(&d1);
		return b ? *h : d2;
	}
	template<>
	inline core_device_t &bselect(bool b, netlist_state_t &d1, core_device_t &d2)
	{
		plib::unused_var(d1);
		if (b)
			throw nl_exception("bselect with netlist and b==true");
		return d2;
	}

	NETLIB_OBJECT(twoterm)
	{
		// FIXME locate use case of owned = true and eliminate them if possible
		NETLIB_CONSTRUCTOR_EX(twoterm, bool terminals_owned = false)
		, m_P(bselect(terminals_owned, owner, *this), (terminals_owned ? name + "." : "") + "1", &m_N)
		, m_N(bselect(terminals_owned, owner, *this), (terminals_owned ? name + "." : "") + "2", &m_P)
		{
		}

		//NETLIB_UPDATE_TERMINALSI() { }
		//NETLIB_RESETI() { }

	public:

		NETLIB_UPDATEI();

		solver::matrix_solver_t *solver() const noexcept;

		void solve_now() const;

		template <typename F>
		void change_state(F f, netlist_time delay = netlist_time::quantum()) const
		{
			auto *solv(solver());
			if (solv)
				solv->change_state(f, delay);
		}

		void set_G_V_I(nl_fptype G, nl_fptype V, nl_fptype I) const noexcept
		{
			//               GO, GT,        I
			m_P.set_go_gt_I( -G,  G, (  V) * G - I);
			m_N.set_go_gt_I( -G,  G, ( -V) * G + I);
		}

		nl_fptype deltaV() const noexcept
		{
			return m_P.net().Q_Analog() - m_N.net().Q_Analog();
		}

		nl_fptype V1P() const noexcept
		{
			return m_P.net().Q_Analog();
		}

		nl_fptype V2N() const noexcept
		{
			return m_N.net().Q_Analog();
		}

		void set_mat(nl_fptype a11, nl_fptype a12, nl_fptype rhs1,
					 nl_fptype a21, nl_fptype a22, nl_fptype rhs2) const noexcept
		{
			//               GO,  GT,     I
			m_P.set_go_gt_I(a12, a11, rhs1);
			m_N.set_go_gt_I(a21, a22, rhs2);
		}

		/// \brief Get a const reference to the m_P terminal
		///
		/// This is typically called during initialization to connect
		/// terminals.
		///
		/// \returns Reference to m_P terminal.
		const terminal_t &P() const noexcept { return m_P; }

		/// \brief Get a const reference to the m_N terminal
		///
		/// This is typically called during initialization to connect
		/// terminals.
		///
		/// \returns Reference to m_N terminal.
		const terminal_t &N() const noexcept { return m_N; }

		/// \brief Get a reference to the m_P terminal
		///
		/// This call is only allowed from the core. Device code should never
		/// need to call this.
		///
		/// \returns Reference to m_P terminal.
		terminal_t &setup_P() noexcept { return m_P; }

		/// \brief Get a reference to the m_N terminal
		///
		/// This call is only allowed from the core. Device code should never
		/// need to call this.
		///
		/// \returns Reference to m_P terminal.
		terminal_t &setup_N() noexcept { return m_N; }

	private:
		terminal_t m_P;
		terminal_t m_N;

	};


	// -----------------------------------------------------------------------------
	// nld_R
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT_DERIVED(R_base, twoterm)
	{
		NETLIB_CONSTRUCTOR_DERIVED(R_base, twoterm)
		{
		}

		void set_R(nl_fptype R) const noexcept
		{
			const nl_fptype G = plib::reciprocal(R);
			set_mat( G, -G, nlconst::zero(),
					-G,  G, nlconst::zero());
		}

		void set_G(nl_fptype G) const noexcept
		{
			set_mat( G, -G, nlconst::zero(),
					-G,  G, nlconst::zero());
		}

		NETLIB_RESETI();

	protected:
		//NETLIB_UPDATEI();

	};

	NETLIB_OBJECT_DERIVED(R, R_base)
	{
		NETLIB_CONSTRUCTOR_DERIVED(R, R_base)
		, m_R(*this, "R", nlconst::magic(1e9))
		{
		}


	protected:

		//NETLIB_UPDATEI() { }
		NETLIB_RESETI()
		{
			set_R(std::max(m_R(), exec().gmin()));
		}

		NETLIB_UPDATE_PARAMI()
		{
			// FIXME: We only need to update the net first if this is a time stepping net
			change_state([this]()
			{
				set_R(std::max(m_R(), exec().gmin()));
			});
		}

	private:
		param_fp_t m_R;
		// protect set_R ... it's a recipe to desaster when used to bypass the parameter
		using NETLIB_NAME(R_base)::set_R;
		using NETLIB_NAME(R_base)::set_G;
	};

	// -----------------------------------------------------------------------------
	// nld_POT
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT(POT)
	{
		NETLIB_CONSTRUCTOR(POT)
		, m_R1(*this, "_R1")
		, m_R2(*this, "_R2")
		, m_R(*this, "R", 10000)
		, m_Dial(*this, "DIAL", nlconst::half())
		, m_DialIsLog(*this, "DIALLOG", false)
		, m_Reverse(*this, "REVERSE", false)
		{
			register_subalias("1", m_R1.P());
			register_subalias("2", m_R1.N());
			register_subalias("3", m_R2.N());

			connect(m_R2.P(), m_R1.N());

		}

		//NETLIB_UPDATEI();
		NETLIB_RESETI();
		NETLIB_UPDATE_PARAMI();

	private:
		NETLIB_SUB(R_base) m_R1;
		NETLIB_SUB(R_base) m_R2;

		param_fp_t m_R;
		param_fp_t m_Dial;
		param_logic_t m_DialIsLog;
		param_logic_t m_Reverse;
	};

	NETLIB_OBJECT(POT2)
	{
		NETLIB_CONSTRUCTOR(POT2)
		, m_R1(*this, "_R1")
		, m_R(*this, "R", nlconst::magic(10000.0))
		, m_Dial(*this, "DIAL", nlconst::half())
		, m_DialIsLog(*this, "DIALLOG", false)
		, m_Reverse(*this, "REVERSE", false)
		{
			register_subalias("1", m_R1.P());
			register_subalias("2", m_R1.N());

		}

		//NETLIB_UPDATEI();
		NETLIB_RESETI();
		NETLIB_UPDATE_PARAMI();

	private:
		NETLIB_SUB(R_base) m_R1;

		param_fp_t m_R;
		param_fp_t m_Dial;
		param_logic_t m_DialIsLog;
		param_logic_t m_Reverse;
	};

	// -----------------------------------------------------------------------------
	// nld_C
	// -----------------------------------------------------------------------------
#if 1
	NETLIB_OBJECT_DERIVED(C, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED(C, twoterm)
		, m_C(*this, "C", nlconst::magic(1e-6))
		, m_cap(*this, "m_cap")
		{
		}

		NETLIB_IS_TIMESTEP(true)
		NETLIB_TIMESTEPI()
		{
			// G, Ieq
			const auto res(m_cap.timestep(m_C(), deltaV(), step));
			const nl_fptype G = res.first;
			const nl_fptype I = res.second;
			set_mat( G, -G, -I,
					-G,  G,  I);
		}

		NETLIB_RESETI()
		{
			m_cap.setparams(exec().gmin());
		}

		/// \brief Set capacitance
		///
		/// This call will set the capacitance. The typical use case are
		/// are components like BJTs which use this component to model
		/// internal capacitances. Typically called during initialization.
		///
		/// \param val Capacitance value
		///
		void set_cap_embedded(nl_fptype val)
		{
			m_C.set(val);
		}

	protected:
		//NETLIB_UPDATEI();
		//FIXME: should be able to change
		NETLIB_UPDATE_PARAMI() { }

	private:
		param_fp_t m_C;
		generic_capacitor_const m_cap;
	};

#else
	// Code preserved as a basis for a current/voltage controlled capacitor
	NETLIB_OBJECT_DERIVED(C, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED(C, twoterm)
		, m_C(*this, "C", nlconst::magic(1e-6))
		, m_cap(*this, "m_cap")
		{
		}

		NETLIB_IS_TIMESTEP(true)
		NETLIB_TIMESTEPI()
		{
			m_cap.timestep(m_C(), deltaV(), step);
			if (m_cap.type() == capacitor_e::CONSTANT_CAPACITY)
			{
				const nl_fptype I = m_cap.Ieq(m_C(), deltaV());
				const nl_fptype G = m_cap.G(m_C());
				set_mat( G, -G, -I,
						-G,  G,  I);
			}
		}

		NETLIB_IS_DYNAMIC(m_cap.type() == capacitor_e::VARIABLE_CAPACITY)
		NETLIB_UPDATE_TERMINALSI()
		{
			const nl_fptype I = m_cap.Ieq(m_C(), deltaV());
			const nl_fptype G = m_cap.G(m_C());
			set_mat( G, -G, -I,
					-G,  G,  I);
		}

		param_fp_t m_C;
		NETLIB_RESETI()
		{
			m_cap.setparams(exec().gmin());
		}

	protected:
		//NETLIB_UPDATEI();
		//FIXME: should be able to change
		NETLIB_UPDATE_PARAMI() { }

	private:
		//generic_capacitor<capacitor_e::VARIABLE_CAPACITY> m_cap;
		generic_capacitor<capacitor_e::CONSTANT_CAPACITY> m_cap;
	};
#endif
	// -----------------------------------------------------------------------------
	// nld_L
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT_DERIVED(L, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED(L, twoterm)
		, m_L(*this, "L", nlconst::magic(1e-6))
		, m_gmin(nlconst::zero())
		, m_G(nlconst::zero())
		, m_I(nlconst::zero())
		{
			//register_term("1", m_P);
			//register_term("2", m_N);
		}

		NETLIB_IS_TIMESTEP(true)
		NETLIB_TIMESTEPI();
		NETLIB_RESETI();

	protected:
		//NETLIB_UPDATEI();
		NETLIB_UPDATE_PARAMI();

	private:
		param_fp_t m_L;

		nl_fptype m_gmin;
		nl_fptype m_G;
		nl_fptype m_I;
	};

	/// \brief Class representing the diode model paramers.
	///
	///  This is the model representation of the diode model. Typically, SPICE uses
	///  the following parameters. A "Y" in the first column indicates that the
	///  parameter is actually used in netlist.
	///
	///   |NL? |name  |parameter                        |units|default| example|area  |
	///   |:--:|:-----|:--------------------------------|:----|------:|-------:|:----:|
	///   | Y  |IS    |saturation current               |A    |1.0e-14| 1.0e-14|   *  |
	///   |    |RS    |ohmic resistance                 |Ohm  |      0|      10|   *  |
	///   | Y  |N     |emission coefficient             |-    |      1|       1|      |
	///   |    |TT    |transit-time                     |sec  |      0|   0.1ns|      |
	///   |    |CJO   |zero-bias junction capacitance   |F    |      0|     2pF|   *  |
	///   |    |VJ    |junction potential               |V    |      1|     0.6|      |
	///   |    |M     |grading coefficient              |-    |    0.5|     0.5|      |
	///   |    |EG    |band-gap energy                  |eV   |   1.11| 1.11 Si|      |
	///   |    |XTI   |saturation-current temp.exp      |-    |      3|3.0 pn. 2.0 Schottky| |
	///   |    |KF    |flicker noise coefficient        |-    |      0|        |      |
	///   |    |AF    |flicker noise exponent           |-    |      1|        |      |
	///   |    |FC    |coefficient for forward-bias depletion capacitance formula|-|0.5|| |
	///   |    |BV    |reverse breakdown voltage        |V    |infinite|     40|      |
	///   |    |IBV   |current at breakdown voltage     |V    |  0.001|        |      |
	///   |    |TNOM  |parameter measurement temperature|deg C|     27|      50|      |
	///
	class diode_model_t : public param_model_t
	{
	public:
		diode_model_t(device_t &device, const pstring &name, const pstring &val)
		: param_model_t(device, name, val)
		, m_IS(*this, "IS")
		, m_N(*this, "N")
		{}

		value_t m_IS;    //!< saturation current.
		value_t m_N;     //!< emission coefficient.
	};

	// -----------------------------------------------------------------------------
	// nld_D
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT_DERIVED(D, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED_EX(D, twoterm, const pstring &model = "D")
		, m_model(*this, "MODEL", model)
		, m_D(*this, "m_D")
		{
			register_subalias("A", P());
			register_subalias("K", N());
		}

		NETLIB_IS_DYNAMIC(true)
		NETLIB_UPDATE_TERMINALSI();
		NETLIB_RESETI();

	protected:
		//NETLIB_UPDATEI();
		NETLIB_UPDATE_PARAMI();

	private:
		diode_model_t m_model;
		generic_diode<diode_e::BIPOLAR> m_D;
	};

	// -----------------------------------------------------------------------------
	// nld_VS - Voltage source
	//
	// netlist voltage source must have inner resistance
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT_DERIVED(VS, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED(VS, twoterm)
		, m_t(*this, "m_t", nlconst::zero())
		, m_R(*this, "RI", nlconst::magic(0.1))
		, m_V(*this, "V", nlconst::zero())
		, m_func(*this,"FUNC", "")
		, m_compiled(this->name() + ".FUNCC", this, this->state().run_state_manager())
		, m_funcparam({nlconst::zero()})
		{
			register_subalias("P", P());
			register_subalias("N", N());
			if (m_func() != "")
				m_compiled.compile(m_func(), std::vector<pstring>({{pstring("T")}}));
		}

		NETLIB_IS_TIMESTEP(m_func() != "")

		NETLIB_TIMESTEPI()
		{
			m_t += step;
			m_funcparam[0] = m_t;
			this->set_G_V_I(plib::reciprocal(m_R()),
					m_compiled.evaluate(m_funcparam),
					nlconst::zero());
		}

	protected:

		NETLIB_RESETI()
		{
			NETLIB_NAME(twoterm)::reset();
			this->set_G_V_I(plib::reciprocal(m_R()), m_V(), nlconst::zero());
		}

	private:
		state_var<nl_fptype> m_t;
		param_fp_t m_R;
		param_fp_t m_V;
		param_str_t m_func;
		plib::pfunction<nl_fptype> m_compiled;
		std::vector<nl_fptype> m_funcparam;
	};

	// -----------------------------------------------------------------------------
	// nld_CS - Current source
	// -----------------------------------------------------------------------------

	NETLIB_OBJECT_DERIVED(CS, twoterm)
	{
	public:
		NETLIB_CONSTRUCTOR_DERIVED(CS, twoterm)
		, m_t(*this, "m_t", nlconst::zero())
		, m_I(*this, "I", nlconst::one())
		, m_func(*this,"FUNC", "")
		, m_compiled(this->name() + ".FUNCC", this, this->state().run_state_manager())
		, m_funcparam({nlconst::zero()})
		{
			register_subalias("P", P());
			register_subalias("N", N());
			if (m_func() != "")
				m_compiled.compile(m_func(), std::vector<pstring>({{pstring("T")}}));
		}

		NETLIB_IS_TIMESTEP(m_func() != "")
		NETLIB_TIMESTEPI()
		{
			m_t += step;
			m_funcparam[0] = m_t;
			const nl_fptype I = m_compiled.evaluate(m_funcparam);
			const auto zero(nlconst::zero());
			set_mat(zero, zero, -I,
					zero, zero,  I);
		}

	protected:

		NETLIB_RESETI()
		{
			NETLIB_NAME(twoterm)::reset();
			const auto zero(nlconst::zero());
			set_mat(zero, zero, -m_I(),
					zero, zero,  m_I());
		}

		NETLIB_UPDATE_PARAMI()
		{
			// FIXME: We only need to update the net first if this is a time stepping net
			//FIXME: works only for CS without function
			change_state([this]()
			{
				const auto zero(nlconst::zero());
				set_mat(zero, zero, -m_I(),
						zero, zero,  m_I());
			});
		}

	private:
		state_var<nl_fptype> m_t;
		param_fp_t m_I;
		param_str_t m_func;
		plib::pfunction<nl_fptype> m_compiled;
		std::vector<nl_fptype> m_funcparam;
	};

} // namespace analog
} // namespace netlist

#endif // NLD_TWOTERM_H_
