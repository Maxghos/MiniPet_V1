package cl.maxghos.minipet

import androidx.test.core.app.ApplicationProvider
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28])
class ExpansionProtocolTest {
    private fun fixture()=JSONObject(javaClass.getResourceAsStream("/companion.json")!!.bufferedReader().readText())
    @Test fun nativeHabitAndRoutineMatchAndroidFields(){
        val reply=fixture();assertTrue(reply.getBoolean("ok"))
        val companion=reply.getJSONObject("companion")
        assertTrue(companion.getBoolean("clockReady"));assertEquals(20261102,companion.getInt("day"))
        for(flag in listOf("greetings","celebrations","stories"))assertTrue(companion.has(flag))
        val habit=reply.getJSONArray("habits").getJSONObject(0)
        assertEquals("Lectura",habit.getString("title"));assertEquals(3,habit.getInt("targetPerWeek"))
        assertTrue(habit.getBoolean("doneToday"));assertEquals(1,habit.getInt("weekDone"));assertEquals(1,habit.getInt("streak"))
        val routines=reply.getJSONArray("routines")
        val routine=(0 until routines.length()).map{routines.getJSONObject(it)}.first{it.getString("title")=="Mi estudio"}
        assertTrue(routine.getBoolean("focus"));assertFalse(routine.getBoolean("overview"))
        assertEquals("Repasar algebra",routine.getJSONArray("tasks").getString(0))
    }
    @Test fun routineEditorCopiesServerPlanAndPreservesOtherDrafts(){
        val model=MiniPetModel(ApplicationProvider.getApplicationContext())
        val routines=fixture().getJSONArray("routines")
        val routine=(0 until routines.length()).map{routines.getJSONObject(it)}.first{it.getString("title")=="Mi estudio"}
        model.edit{it.copy(habitTitle="Mi siguiente hábito")};model.editRoutine(routine)
        assertEquals("Mi estudio",model.state.routineTitle);assertEquals("Repasar algebra",model.state.routineTasks)
        assertTrue(model.state.routineFocus);assertFalse(model.state.routineOverview)
        routine.put("title","Otro");assertEquals("Mi estudio",model.state.routineTitle)
        model.clearRoutineDraft();assertNull(model.state.routineId)
        assertEquals("Mi siguiente hábito",model.state.habitTitle)
    }
}
