package fun.scop.app.an.solver

import fun.scop.app.an.util._
import fun.scop.sat._
import fun.scop.app.an.util.BioLqmWrapper

class Saf(
    an: AutomataNetwork,
    option: String = "full",
    satsolver: SatSolver
) extends AbstractSolver() {

  var dimacsIndex = 0
  var nofClauses = 0

  var tij: Map[(Transition, Int, Int), Int] = Map.empty
  var ti: Map[(Transition, Int), Int] = Map.empty
  var tj: Map[(Transition, Int), Int] = Map.empty
  var t: Map[(Transition), Int] = Map.empty
  var xx: Map[(Automata, Int, Int), Int] = Map.empty
  var cyc: Map[(Automata, Seq[(Int, Int)]), Int] = Map.empty

  var eq: Map[(Automata, Int, Int), Int] = Map.empty
  var le: Map[(Automata, Int, Int), Int] = Map.empty

  // var d: Map[(Int, Int), Int] = Map.empty

  var dmap: Map[Int, String] = Map.empty

  private def issueAuxVar() = {
    dimacsIndex += 1
    dimacsIndex
  }

  private def makeAutomataVar(k: Int) = {
    for (a <- an.automatas; s <- 1 to k) {
      // println(s"=== $a ${an.domain(a)}")
      if (an.domain(a).size > 2) {
        for (v <- an.domain(a)) {
          // println(s"===== $v")
          dimacsIndex += 1
          xx += (a, s, v) -> dimacsIndex
          dmap += dimacsIndex -> s"x($a,$s,$v)"
          // println(s"x($a,$s,$v)")
        }
      } else {
        dimacsIndex += 1
        xx += (a, s, 0) -> -dimacsIndex
        xx += (a, s, 1) -> dimacsIndex
        dmap += dimacsIndex -> s"x($a,$s,1)"
      }
    }
  }

  private def x(a: Automata, i: Int, v: Int) = xx(a, i, v)

  private def makeTransitionVar(k: Int) = {
    for (
      tr <- an.transitions;
      s1 <- 1 to k;
      s2 <- 1 to k if s1 != s2
    ) {
      dimacsIndex += 1
      tij += (tr, s1, s2) -> dimacsIndex
      dmap += dimacsIndex -> s"tij($tr,$s1,$s2)"
    }
    for (tr <- an.transitions; i <- 1 to k) {
      dimacsIndex += 1
      ti += (tr, i) -> dimacsIndex
      dmap += dimacsIndex -> s"ti($t,$i)"
    }

  }

  private def makeIntegratedTransitionVar(k: Int) = {
    for (tr <- an.transitions) {
      dimacsIndex += 1
      t += tr -> dimacsIndex
      dmap += dimacsIndex -> s"t($t)"
    }
    // for (tr <- an.transitions; i <- 1 to k) {
    //   dimacsIndex += 1
    //   ti += (tr, i) -> dimacsIndex
    //   dmap += dimacsIndex -> s"ti($t,$i)"
    // }
    for (tr <- an.transitions; j <- 1 to k) {
      dimacsIndex += 1
      tj += (tr, j) -> dimacsIndex
      dmap += dimacsIndex -> s"tj($t,$j)"
    }

  }

  private def makeEqVar(k: Int) = {
    for (a <- an.automatas) {
      for (i <- 1 to k; j <- i + 1 to k) {
        dimacsIndex += 1
        eq += (a, i, j) -> dimacsIndex
        dmap += dimacsIndex -> s"eq($a,$i,$j)"
      }
    }
  }

  private def makeEqDef(a: Automata, i: Int, j: Int) = {
    // right to left
    // if i = j then eq=true
    if (an.domain(a).size > 2) { // for boolean domain variables
      (for (d <- an.domain(a))
        yield Seq(
          Seq(eq(a, i, j), -x(a, i, d), -x(a, j, d)),
          Seq(eq(a, i, j), x(a, i, d), x(a, j, d))
        )).flatten
    } else { // for others
      Seq(
        Seq(eq(a, i, j), -x(a, i, 1), -x(a, j, 1)),
        Seq(eq(a, i, j), x(a, i, 1), x(a, j, 1))
      )
    }
  }

  private def makeEqDefinitionConstraint(k: Int) = {
    for (a <- an.automatas) {
      for (i <- 1 to k; j <- i + 1 to k) {
        makeEqDef(a, i, j).foreach(addClause)
      }
    }
  }

  private def atMostOne(ps: Seq[Int]): Seq[Seq[Int]] =
    ps.map(_ * -1).combinations(2).toSeq

  private def atLeastOne(ps: Seq[Int]): Seq[Seq[Int]] = Seq(ps)

  private def exactOne(ps: Seq[Int]): Seq[Seq[Int]] =
    atMostOne(ps) ++ atLeastOne(ps)

  private def addAllClauses(cs: Seq[Seq[Int]]) =
    cs.foreach(addClause)

  private def addClause(lits: Seq[Int]) = {
    if (false) {
      val ss = for {
        l <- lits
        p = math.abs(l)
        str = if (l < 0) s"-${dmap(p)}" else s"${dmap(p)}"
      } yield str
    }
    nofClauses += 1
    // println(s"${lits.mkString(" ")} 0")
    satsolver.addClause(lits)
  }

  private def makeIntVarEncoding(k: Int) = {
    for (a <- an.automatas; s <- 1 to k if an.domain(a).size > 2)
      DirectVarEncoding(a, s)
  }

  private def DirectVarEncoding(a: Automata, s: Int) =
    addAllClauses(exactOne(an.domain(a).map(d => x(a, s, d))))

  private def makeDefLocalTransition(k: Int) = {
    for (tr <- an.transitions; i <- 1 to k) {
      // left to right --->
      // if transition is fired then its pre conditions must be satisfied
      for (av <- tr.pre) {
        addClause(Seq(-ti(tr, i), x(av.a, i, av.v)))
      }
      // right to left <---
      addClause(ti(tr, i) +: tr.pre.map(av => -x(av.a, i, av.v)))
    }
  }

  private def makeDefGlobalTransition(k: Int) = {
    // left to right --->
    for (tr <- an.transitions; i <- 1 to k; j <- 1 to k if i != j) {
      addClause(Seq(-tij(tr, i, j), ti(tr, i)))
      addClause(Seq(-tij(tr, i, j), x(tr.suc.a, j, tr.suc.v)))
      for (a <- an.automatas; clause <- eqc(a, i, j) if tr.suc.a != a) {
        addClause(-tij(tr, i, j) +: clause)
      }
    }
    // right to left <--- 必須ではない
  }

  private def makeDefTrapDomain(k: Int) = {
    for (tr <- an.transitions; i <- 1 to k) {
      val rhs = (1 to k).filter(j => i != j).map(j => tij(tr, i, j))
      addClause(-ti(tr, i) +: rhs)
    }
  }

  private def eqc(a: Automata, i: Int, j: Int): Seq[Seq[Int]] = {
    if (an.domain(a).size > 2) {
      (for (d <- an.domain(a))
        yield Seq(
          Seq(-x(a, i, d), x(a, j, d)),
          Seq(x(a, i, d), -x(a, j, d))
        )).flatten
    } else {
      Seq(Seq(-x(a, i, 1), x(a, j, 1)), Seq(x(a, i, 1), -x(a, j, 1)))
    }
  }

  private def makeLeClauses(a: Automata, i: Int, j: Int) = {

    if (an.domain(a).size == 2) {
      Seq(Seq(-x(a, i, 1), x(a, j, 1)))
    } else {
      for {
        di <- an.domain(a);
        dj <- an.domain(a)
        if !(di <= dj)
      } yield Seq(-x(a, i, di), -x(a, j, dj))
    }

  }

  private def makeLtClauses(a: Automata, i: Int, j: Int) = {

    if (an.domain(a).size == 2) {
      Seq(Seq(-x(a, i, 1)))
    } else {
      for {
        di <- an.domain(a);
        dj <- an.domain(a)
        if !(di < dj)
      } yield Seq(-x(a, i, di), -x(a, j, dj))

    }

  }
  private def makeLexConstraint(i: Int, j: Int) = {

    for (n <- 1 to an.automatas.size) {
      val init = an.automatas.take(n).init
      val last = an.automatas.take(n).last

      val initLits: Seq[Int] = init.map(a => -eq(a, i, j))

      if (n != an.automatas.size) {
        // xi = xj and ... -> xi <= xj
        // val lastLits: Seq[Int] = Seq(-x(last, i, 1), x(last, j, 1))
        makeLeClauses(last, i, j).foreach { clause =>
          addClause(initLits ++ clause)
        }
      } else {
        // xi = xj and ... -> xi < xj
        // val lastLits: Seq[Int] = Seq(-x(last, i, 1))
        makeLtClauses(last, i, j).foreach { clause =>
          addClause(initLits ++ clause)
        }
        // addClause(initLits ++ lastLits)
      }
    }

  }

  private def makeSymmetryBreaking(k: Int) = {
    for (i <- 1 until k) {
      makeLexConstraint(i, i + 1)
    }
  }
  private def cycleCondition(a: Automata) =
    !an.automataDoesNotHaveTransition.contains(a) &&
      !an.automataDoesNotHaveCycle(a)

  private def makeCycleVar(k: Int) = {

    for (a <- an.automatas if cycleCondition(a)) {
      for (cycle <- an.automata2cycles(a)) {
        dimacsIndex += 1
        cyc += (a, cycle) -> dimacsIndex
        dmap += dimacsIndex -> s"cyc($a,$cycle)"
      }
    }

  }

  private def cycleThenTransition(a: Automata, cycle: Seq[(Int, Int)]) = {
    val tvars =
      for ((u, v) <- cycle)
        yield
          if (an.arc2transitions(a, u, v).size == 1)
            Seq(t(an.arc2transitions(a, u, v).head))
          else
            an.arc2transitions(a, u, v).map(tr => t(tr))

    // println(s"${a.name} ${cycle.mkString(" ")} ${tvars.mkString(" ")}")
    for (tvar <- tvars) {

      val c = -cyc(a, cycle) +: tvar
      // println(s"${c.mkString(" ")}")
      addClause(c)
    }

  }

  private def transitionThenCycle(
      a: Automata,
      ca: Set[Seq[(Int, Int)]],
      tr: Transition
  ) = {
    val cyclesContainTr = ca.filter(cycle =>
      cycle.exists { case (u, v) => an.arc2transitions(a, u, v).contains(tr) }
    )

    val lits = cyclesContainTr.map { cycle => cyc(a, cycle) }.toSeq
    // println(
    //   s"${a.name} ${ca.mkString(" ")}, $tr ${cyclesContainTr
    //       .mkString(" ")} ${lits.mkString(" ")}"
    // )
    val c = -t(tr) +: lits
    // println(s"${c.mkString(" ")}")
    addClause(c)
  }

  private def makeCycleConstraint(k: Int) = {

    for (a <- an.automatas if cycleCondition(a)) {

      // println(s"=== $a")

      val ta = an.transitions.filter(tr => tr.target == a)
      val ca = an.automata2cycles(a)

      // println(ta.mkString(","))
      // println(ca.mkString(","))

      // left to right
      for (cycle <- ca)
        cycleThenTransition(a, cycle)

      // right to left
      for (tr <- ta)
        transitionThenCycle(a, ca, tr)

      // if (ca.size > 1) {
      //   // at-most-one を加える
      //   val clauses = atMostOne(
      //     an.automata2cycles(a).map(cycle => cyc(a, cycle)).toSeq
      //   )
      //   clauses.foreach(addClause)
      // }
    }
    // val allcycles = an.automatas
    //   .filter(cycleCondition(_))
    //   .flatMap(a => an.automata2cycles(a).map(cycle => cyc(a, cycle)))
    // addClause(allcycles)
  }

  private def makeIntegratedTransitionDefinition(k: Int) = {
    //  transitions coming from state i
    //  (for each i) ti <-> \bigvee_{j} tij
    for (tr <- an.transitions; i <- 1 to k) {
      // addClause(-ti(tr, i) +: (1 to k).filter(_ != i).map(j => tij(tr, i, j)))
      for (j <- 1 to k if j != i) {
        addClause(Seq(ti(tr, i), -tij(tr, i, j)))
      }
    }
    //  transitions going to state j
    // (for each j) tj <-> \bigvee_{i} tij
    for (tr <- an.transitions; j <- 1 to k) {
      addClause(-tj(tr, j) +: (1 to k).filter(_ != j).map(i => tij(tr, i, j)))
      for (i <- 1 to k if i != j) {
        addClause(Seq(tj(tr, j), -tij(tr, i, j)))
      }
    }
    // transitions t
    // t <-> \bigvee_{ij} tij
    for (tr <- an.transitions) {
      addClause(
        -t(tr) +:
          ((1 to k).map(i => ti(tr, i)))
      )
      addClause(
        -t(tr) +:
          (1 to k).map(j => tj(tr, j))
      )
      for (i <- 1 to k)
        addClause(Seq(t(tr), -ti(tr, i)))
      for (j <- 1 to k)
        addClause(Seq(t(tr), -tj(tr, j)))

    }

  }

  private def makeAtleastOneTransition(k: Int) = {
    for (i <- 1 to k) {
      addClause(an.transitions.map(t => ti(t, i)))
      addClause(an.transitions.map(t => tj(t, i)))
    }
  }

  private def makeTransitionNotActive(k: Int) = {
    // transition
    for (tr <- an.inactiveTransition) {
      addClause(Seq(-t(tr)))
    }
  }

  private def makeAutomataConstant(k: Int) = {
    // cycle を持たないやつは全部同じも追加する
    for (a <- an.automataDoesNotHaveCycle) {
      for (i <- 1 until k) {
        addClause(Seq(eq(a, i, i + 1)))
      }
    }
  }

  private def encode(k: Int, foundSoFar: Seq[Attractor]) = {
    /* defining variables */
    makeAutomataVar(k)
    makeTransitionVar(k)

    /* clauses of definition */
    makeIntVarEncoding(k)

    /* clauses of constraints */
    makeDefLocalTransition(k)
    makeDefGlobalTransition(k)
    makeDefTrapDomain(k)

    /* k >= 2 optional */
    if (k >= 2) {
      makeEqVar(k)
      makeEqDefinitionConstraint(k)

      makeIntegratedTransitionVar(k)
      makeIntegratedTransitionDefinition(k)
      makeAtleastOneTransition(k)

      if (an.isCycleComputed) {
        makeTransitionNotActive(k)
        makeAutomataConstant(k)
      }

      option match {
        case "full" if an.isCycleComputed => {
          makeCycleVar(k)
          makeCycleConstraint(k)
          makeSymmetryBreaking(k)
        }
        case "full" => {
          makeSymmetryBreaking(k)
        }
        case "symmetry" => {
          makeSymmetryBreaking(k)
        }
        case "cycle" => {
          makeCycleVar(k)
          makeCycleConstraint(k)
        }
        case "basic" => {}
      }

    }

    /* blocking foundSoFar */
    for (fsf <- foundSoFar if fsf.gs.size > 1)
      blockAttractor(fsf, k)
  }

  private def getValueOfAutomata(a: Automata, s: Int): Int = {
    if (an.domain(a).size < 3)
      if (satsolver.model(x(a, s, 1)) < 0) 0 else 1
    else {
      for (d <- an.domain(a)) {
        if (satsolver.model(x(a, s, d)) > 0)
          return d
      }
      -1
    }
  }

  private def getAttractorFromModel(k: Int) = {
    Attractor(
      (1 to k).map(i =>
        GlobalState(
          an.automatas.map(a => AutomataValued(a, getValueOfAutomata(a, i)))
        )
      )
    )
  }

  private def blockAttractor(att: Attractor, k: Int) = {
    for (i <- 1 to k)
      addClause(att.gs.head.avs.map(av => -x(av.a, i, av.v)))
  }

  private def printTransitionFromModel(k: Int) = {
    for {
      i <- 1 to k
      j <- 1 to k
      if i != j
      tr <- an.transitions
      if satsolver.model(tij(tr, i, j)) > 0
    } {
      println(s"$i -> $j $tr")
    }
  }

  def findAttractorsLeK(
      k: Int,
      attractorsFoundSoFar: Seq[Attractor]
  ): Seq[Attractor] = {

    var modelCounter = 0
    encode(k, attractorsFoundSoFar)

    var attractorsFound = attractorsFoundSoFar
    var result = false

    println(s"k: $k, #Var: ${dimacsIndex}, #Clause: ${nofClauses}")
    while (satsolver.solve.get) {
      result = true
      modelCounter += 1
      println(s"#$modelCounter")
      val attractor = getAttractorFromModel(k)
      // printTransitionFromModel(k)
      // println(attractor)
      println(
        attractor.gs.seq
          .map(g => g.avs.map(av => av.v).mkString(""))
          .mkString(" ")
      )
      if (k > 1)
        attractorsFound = attractor +: attractorsFound
      blockAttractor(attractor, k)
    }
    if (!result) println("None")

    attractorsFound
  }

  def findAttractorsLeKApp(
      k: Int,
      attractorsFoundSoFar: Seq[Attractor]
  ): Seq[Attractor] = {

    var modelCounter = 0
    encode(k, attractorsFoundSoFar)

    var attractorsFound = attractorsFoundSoFar
    var result = false

    println(s"k: $k, #Var: ${dimacsIndex}, #Clause: ${nofClauses}")
    while (satsolver.solve.get) {
      result = true
      modelCounter += 1
      println(s"#$modelCounter")
      val attractor = getAttractorFromModel(k)
      // printTransitionFromModel(k)
      // println(attractor)
      println(
        attractor.gs.seq
          .map(g => g.avs.map(av => av.v).mkString(""))
          .mkString(" ")
      )

      attractorsFound = attractor +: attractorsFound
      blockAttractor(attractor, k)
    }
    if (!result) println("None")

    attractorsFound
  }

}

object Saf {

  var help = false
  var libname: Option[String] = None
  var libpath: Option[String] = None
  var k1solver: Option[String] = None
  var k = Int.MaxValue
  var option = "full"
  var isBooleanNetwork = false

  def parseOptions(arguments: List[String]): List[String] = arguments match {
    case "-h" :: rest => {
      help = true
      parseOptions(rest)
    }
    case "--help" :: rest => {
      help = true
      parseOptions(rest)
    }
    case "-libname" :: name :: rest => {
      libname = Some(name)
      parseOptions(rest)
    }
    case "-libpath" :: path :: rest => {
      libpath = Some(path)
      parseOptions(rest)
    }
    case "-k1solver" :: path :: rest => {
      k1solver = Some(path)
      parseOptions(rest)
    }
    case "-k" :: n :: rest => {
      k = n.toInt
      parseOptions(rest)
    }
    case "-encode" :: op :: rest => {
      option = op
      parseOptions(rest)
    }
    case "-isbool" :: rest => {
      isBooleanNetwork = true
      parseOptions(rest)
    }
    case _ => arguments
  }

  def showOptions = {
    println("\t-h									: show this help")
    println("\t--help									: show this help")
    println("\t-libname <Library Name>							: name of IPASIR Library")
    println(
      "\t-libpath <Library Path>							: directory where IPASIR Library is"
    )
    println(
      "\t-k1solver <K1 Solver Path>						: path of the executable of SAT solver for k=1"
    )
    println("\t-k <INT>								: upper bound of k (default Int.MaxValue)")
    println(
      "\t-encode <cycle|symmetry|full>						: encoding option (default full)"
    )
  }

  private def launchAnIpasirBddSolver(
      an: AutomataNetwork,
      libname: String,
      libpath: String,
      sat2bdd: String
  ) = {
    println(s"Using IPASIR SAT Solver and AllSAT Solver")
    println(s"Libpath: $libpath")
    println(s"Libname: $libname")
    println(s"K1solver: $sat2bdd")
    println(s"\n${an.automatas.map(a => a.name).mkString(",")}")

    var foundSoFar = Seq.empty[Attractor]
    for (i <- 1 to k) {
      val solver =
        if (i != 1)
          new Saf(
            an,
            option,
            new PureIpasirSatSolver(libname, libpath)
          )
        else new SafK1(an, sat2bdd)

      val foundAtI = solver.findAttractorsLeK(i, foundSoFar)
      foundSoFar = foundSoFar ++ foundAtI
    }
  }
  private def launchAnIpasirSolver(
      an: AutomataNetwork,
      libname: String,
      libpath: String
  ) = {
    println(s"Using IPASIR SAT Solver")
    println(s"Libpath: $libpath")
    println(s"Libname: $libname")
    println(s"\n${an.automatas.map(a => a.name).mkString(",")}")

    var foundSoFar = Seq.empty[Attractor]
    for (i <- 1 to k) {
      val solver =
        new Saf(an, option, new PureIpasirSatSolver(libname, libpath))
      val foundAtI = solver.findAttractorsLeK(i, foundSoFar)
      foundSoFar = foundSoFar ++ foundAtI
    }
  }

  private def launchAnJavaBddSolver(
      an: AutomataNetwork,
      sat2bdd: String
  ) = {
    println(s"Using Sat4j SAT Solver and AllSAT Solver")
    println(s"K1solver: $sat2bdd")
    println(s"\n${an.automatas.map(a => a.name).mkString(",")}")

    var foundSoFar = Seq.empty[Attractor]
    for (i <- 1 to k) {
      val solver =
        if (i != 1) new Saf(an, option, new Sat4j())
        else new SafK1(an, sat2bdd)

      val foundAtI = solver.findAttractorsLeK(i, foundSoFar)
      foundSoFar = foundSoFar ++ foundAtI
    }

  }

  private def launchAnJavaSolver(an: AutomataNetwork) = {
    println(s"Using Sat4j SAT Solver")

    println(s"\n${an.automatas.map(a => a.name).mkString(",")}")

    var foundSoFar = Seq.empty[Attractor]
    for (i <- 1 to k) {
      val solver = new Saf(an, option, new Sat4j())
      val foundAtI = solver.findAttractorsLeK(i, foundSoFar)
      foundSoFar = foundSoFar ++ foundAtI
    }

  }

  def getAN(inputFilePath: String): AutomataNetwork = {

    val ext = inputFilePath.split('.').last

    if (ext == "an") {
      val anParser = AutomataNetworkParser(inputFilePath)
      anParser.parse()
    } else {
      val bioLQM = new BioLqmWrapper()
      val anFilePath =
        bioLQM.convertOtherNetworkToAutomataNetwork(inputFilePath)
      val anParser = AutomataNetworkParser(anFilePath)
      new java.io.File(anFilePath).deleteOnExit()
      anParser.parse()
    }

  }

  def main(args: Array[String]): Unit = {

    val input: String = parseOptions(args.toList) match {
      case file :: Nil if !help =>
        file
      case _ => {
        if (!help)
            println(s"Something wrong in arguments: ${args.mkString(" ")}")
        println(
          "Usage: java -jar saf.jar [options] [inputFile]"
        )
        showOptions
        System.exit(1)
        ""
      }
    }

    // val anParser = AutomataNetworkParser(input)

    val an = getAN(input) // anParser.parse()
    an.calcCycles()

    (libname.isDefined, libpath.isDefined, k1solver.isDefined) match {
      case (true, true, true) =>
        launchAnIpasirBddSolver(an, libname.get, libpath.get, k1solver.get)
      case (true, true, false) =>
        launchAnIpasirSolver(an, libname.get, libpath.get)
      case (false, false, false) =>
        launchAnJavaSolver(an)
      case (false, false, true) =>
        launchAnJavaBddSolver(an, k1solver.get)
      case (true, false, _) => {
        println(s"libname and libpath must be given together.")
        System.exit(1);
      }
      case (false, true, _) => {
        println(s"libname and libpath must be given together.")
        System.exit(1);
      }
    }

  }
}
