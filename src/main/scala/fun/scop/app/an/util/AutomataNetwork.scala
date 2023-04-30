package fun.scop.app.an.util

case class AutomataNetwork(
    automatas: Seq[Automata],
    domain: Map[Automata, Seq[Int]],
    transitions: Seq[Transition]
) {

  var isCycleComputed = true

  val arcs2cycleList: Map[Set[Tuple2[Int, Int]], Set[Seq[Tuple2[Int, Int]]]] =
    Map(
      Set((0, 1), (0, 2), (1, 0), (2, 0)) -> Set(
        Seq((0, 1), (1, 0)),
        Seq((0, 2), (2, 0))
      ),
      Set((0, 1), (1, 0)) -> Set(Seq((0, 1), (1, 0))),
      Set((0, 1), (1, 0), (1, 2), (2, 1)) -> Set(
        Seq((0, 1), (1, 0)),
        Seq((1, 2), (2, 1)),
        Seq((0, 1), (1, 0), (1, 2), (2, 1))
      ),
      Set((0, 1), (1, 2), (2, 1)) -> Set(Seq((1, 2), (2, 1))),
      Set((0, 1), (1, 0), (2, 1)) -> Set(Seq((0, 1), (1, 0))),
      Set((0, 1), (0, 2), (1, 0)) -> Set(Seq((0, 1), (1, 0))),
      Set((0, 1), (0, 2), (1, 2), (2, 0)) -> Set(
        Seq((0, 1), (1, 2), (2, 0)),
        Seq((0, 2), (2, 0))
      ),
      Set((3, 2), (0, 1), (2, 3), (1, 2), (2, 1), (1, 0)) -> Set(
        Seq((0, 1), (1, 0)),
        Seq((1, 2), (2, 1)),
        Seq((2, 3), (3, 2)),
        Seq((0, 1), (1, 2), (2, 1), (1, 0)),
        Seq((1, 2), (2, 1), (2, 3), (3, 2)),
        Seq((0, 1), (1, 0), (2, 3), (3, 2)),
        Seq((0, 1), (1, 2), (2, 1), (1, 0), (2, 3), (3, 2))
      )
    )
  var automata2cycles: Map[Automata, Set[Seq[(Int, Int)]]] = Map.empty
  var automataDoesNotHaveTransition: Set[Automata] = Set.empty
  var automataDoesNotHaveCycle: Set[Automata] = Set.empty
  var arc2transitions: Map[(Automata, Int, Int), Seq[Transition]] = Map.empty

  var inactiveTransition: Set[Transition] = Set.empty
  // private def cycleOfAutomata = {}

  def calcCycles(): Unit = {

    // LOOP: for each automata
    for (a <- automatas) {

      // all transitions make automata a different
      val transitionsForA = transitions.filter(t => t.target == a)

      // all arcs given by the transitions
      val arcs = transitionsForA.map(t => (t.origin.v, t.destination.v)).toSet

      // make a mappping from an arc to transitions that have the arc
      for ((u, v) <- arcs) {
        val ts =
          transitionsForA
            .filter(t => t.origin.v == u && t.destination.v == v)
            .toSeq
        // println(s"$a ${ts.mkString(",")}")
        arc2transitions += (a, u, v) -> ts
      }

      // check the status of arcs
      if (arcs2cycleList.contains(arcs)) { // サイクルがある場合
        /*         println(s"== Cycles of Automata $a ==")
        for (cycle <- arcs2cycleList(arcs)) {
          println(s"${cycle.mkString(",")}")
        }
         */
        automata2cycles += a -> arcs2cycleList(arcs)
      } else if (arcs.isEmpty) { // a を変化させる transition がない場合
        // println(s"There is no transion targetting $a")
        automataDoesNotHaveTransition += a
      } else if (arcs.size == 1) {

        automataDoesNotHaveCycle += a
        val (u, v) = arcs.head
        arc2transitions(a, u, v).foreach { tr =>
          inactiveTransition += tr
        }
      } else { // map に登録されていない --> サイクルを構成しない場合 (0,1) だけ， とか， (1,0) だけ とか
        isCycleComputed = false
        return ()
        // throw new Exception(s"$arcs is not in arcs2cycleList")
        // println(s"Error. Please modify arcs2cycleList. ${arcs.mkString(",")}")
      }

    }
  }

  def PreOfTs_conflictWith_PreOf(t: Transition) =
    transitions.filter(t0 =>
      t0.pre.exists(v0 => t.pre.exists(v1 => v0.conflictWith(v1)))
    )

  def SucOfTs_conflictWith_PreOf(t: Transition) =
    transitions.filter(t0 =>
      t0.sucsuc.exists(v0 => t.pre.exists(v1 => v0.conflictWith(v1)))
    )

  def PreOfTs_conflictWith_SucOf(t: Transition) =
    transitions.filter(t0 =>
      t0.pre.exists(v0 => t.sucsuc.exists(v1 => v0.conflictWith(v1)))
    )

  def SucOfTs_conflictWith_SucOf(t: Transition) =
    transitions.filter(t0 =>
      t0.sucsuc.exists(v0 => t.sucsuc.exists(v1 => v0.conflictWith(v1)))
    )

  def tsNotPlayableAtaTimeWith(t: Transition) =
    transitions.filter(t0 =>
      t0.origin != t.origin || t0.destination != t.destination || t0.pre.exists(
        v0 => t.pre.exists(v => v0.conflictWith(v))
      )
    )

  val ppMap = transitions.map(t => t -> PreOfTs_conflictWith_PreOf(t)).toMap
  val spMap = transitions.map(t => t -> SucOfTs_conflictWith_PreOf(t)).toMap
  val psMap = transitions.map(t => t -> PreOfTs_conflictWith_SucOf(t)).toMap
  val ssMap = transitions.map(t => t -> SucOfTs_conflictWith_SucOf(t)).toMap

  val npMap = transitions.map(t => t -> tsNotPlayableAtaTimeWith(t)).toMap

  val exclusiveByPre: Seq[(Transition, Transition)] = for {
    i <- 0 until transitions.size
    j <- i + 1 until transitions.size
    overlap = transitions(i).pre
      .map(_.a)
      .toSet
      .intersect(transitions(j).pre.map(_.a).toSet)
    if overlap.exists(a =>
      transitions(i).getPreValue(a) != transitions(j).getPreValue(a)
    )
  } yield (transitions(i), transitions(j))

  val exclusiveBySuc: Seq[(Transition, Transition)] = for {
    i <- 0 until transitions.size
    j <- i + 1 until transitions.size
    if transitions(i).destination.a == transitions(
      j
    ).destination.a && transitions(i).destination.v != transitions(
      j
    ).destination.v
  } yield (transitions(i), transitions(j))

  def stats = Map(
    "Number of Automata: " -> automatas.size,
    "Number of Transition: " -> transitions.size,
    "Range of State: " -> domain.values.map(_.size).max
  )

  def toAnFormat = {
    def am(a: Automata) = """"""" + s"${a.name}" + """""""
    def amv(a: Automata, v: Int) = s"${am(a)}=$v"

    val s = new StringBuilder
    val dq = '\"'

    // automatas
    for (a <- automatas) {
      val dom = domain(a).mkString("[", ",", "]")
      s.append(s"${am(a)} ${dom}\n")
    }

    s.append(s"\n")

    // transitions
    for (tr <- transitions) {
      val a = tr.origin.a
      val v0 = tr.origin.v
      val v1 = tr.destination.v

      val condition = tr.condition.map(av => amv(av.a, av.v)).mkString(" and ")

      s.append(
        s"${am(a)} $v0 -> $v1 " +
          s"${if (condition.nonEmpty) s"when $condition" else ""}\n"
      )
    }

    s.result
  }

}

object AutomataNetwork {

  def fromFile(file: String) = {
    val anParser = AutomataNetworkParser(file)
    anParser.parse()
  }

  def main(args: Array[String]): Unit = {
    val anParser = AutomataNetworkParser(
      // "/Users/soh/01_paper/121_オートマタネットワーク/benchmark/amb17example.an"
      args(0)
    )
    val an = anParser.parse()

    println(an)

    // i, j で mapt(t,i,j) が発火するための必要条件
    for (t0 <- an.transitions) {
      println("==========")
      println(t0)
      println("==========")
      an.ppMap(t0).foreach(println)
      println("-")
      an.spMap(t0).foreach(println)
      println("-")
      an.psMap(t0).foreach(println)
      println("-")
      an.ppMap(t0).foreach(println)
      println("-")
      an.tsNotPlayableAtaTimeWith(t0).foreach(println)
    }
  }

}
