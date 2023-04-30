package fun.scop.app.an.util

case class Transition(
    origin: AutomataValued,
    destination: AutomataValued,
    condition: Seq[AutomataValued]
) {

  val target = origin.a

  private val cmap = (origin +: condition).map(c => (c.a, c.v)).toMap
  val conditionMap = condition.map(c => (c.a, c.v)).toMap

  def getPreValue(a: Automata) = cmap(a)

  def getSucValue(a: Automata) = {
    require(a == destination.a)
    destination.v
  }

  def pre = origin +: condition

  def suc = destination

  def sucsuc = destination +: condition

  def isFireableFrom(gs: GlobalState) = {
    pre.forall(gs.avs.contains)
  }

  /*
  def isFireableBetween(g1: GlobalState, g2: GlobalState) = {

    require(g1.an == g2.an)
    val c1 = isFireableFrom(g1)
    val allExceptDest = g2.an.as.filter(_ != destination.a)
    val c2 = allExceptDest.forall(a => g1.gs(a) == g2.gs(a))
    val c3 = destination.v == g2.gs(destination.a)

    c1 && c2 && c3
  }
   */

  override def toString =
    s"Tr(${origin.a.name}=${origin.v}->${destination.v}[${condition.mkString(",")}])" // s"${origin.a.name}(${origin.v}->${destination.v}):${condition.map(c => s"${c.a.name}(${c.v})").mkString(";")}"
}
